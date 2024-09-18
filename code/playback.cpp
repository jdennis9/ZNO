/*
    ZNO Music Player
    Copyright (C) 2024  Jamie Dennis

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#include "playback.h"
#include "platform.h"
#include "audio.h"
#include "main.h"
#include <sndfile.h>
#include <samplerate.h>
#include <math.h>
#include <string.h>

struct Decoder {
    SNDFILE *file;
    SRC_STATE *resampler;
    SF_INFO info;
    i64 frame_index;
};

static Audio_Stream g_stream;
static Decoder g_decoder;
static Mutex g_lock;
static bool g_paused;

static void close_decoder(Decoder *dec) {
    if (dec->file) sf_close(dec->file);
    if (dec->resampler) src_delete(dec->resampler);
    *dec = Decoder{};
}

static bool open_decoder(Decoder *dec, const wchar_t *filename) {
    close_decoder(dec);
    dec->file = sf_wchar_open(filename, SFM_READ, &dec->info);
    if (!dec->file) return false;
    
    return true;
}

//@TODO: Some tracks have a click after the first buffer is filled, fix it
void audio_stream_callback(void *user_data, f32 *output_buffer, const Audio_Buffer_Spec *spec) {
    lock_mutex(g_lock);
    defer(unlock_mutex(g_lock));
    
    if (g_paused || !g_decoder.file) {
        zero_array(output_buffer, spec->channel_count * spec->frame_count);
        return;
    }
    
    Decoder *dec = (Decoder*)user_data;
    bool needs_resampling = dec->info.samplerate != spec->sample_rate;
    
    zero_array(output_buffer, spec->frame_count * spec->channel_count);
    
    if (!needs_resampling) {
        sf_count_t frames_read = sf_readf_float(dec->file, output_buffer, spec->frame_count);
        if (frames_read < spec->frame_count) {
            notify(NOTIFY_END_OF_TRACK);
        }
    }
    else {
        if (!dec->resampler) {
            int error;
            dec->resampler = src_new(SRC_SINC_FASTEST, spec->channel_count, &error);
            log_debug("Creating resampler...\n");
        }
        
        SRC_DATA src = {};
        f32 in_to_out_sample_ratio = (f32)spec->sample_rate/(f32)dec->info.samplerate;
        i32 input_frame_count = (i32)ceilf(spec->frame_count / in_to_out_sample_ratio);
        f32 *pre_resample_buffer = (f32*)malloc(input_frame_count * spec->channel_count * sizeof(f32));
        defer(free(pre_resample_buffer));
        
        sf_count_t frames_read = sf_readf_float(dec->file, pre_resample_buffer, input_frame_count);
        if (frames_read < input_frame_count) {
            notify(NOTIFY_END_OF_TRACK);
        }
        
        src.data_in = pre_resample_buffer;
        src.data_out = output_buffer;
        src.input_frames = input_frame_count;
        src.output_frames = spec->frame_count;
        src.src_ratio = (f64)in_to_out_sample_ratio;
        
        src_process(dec->resampler, &src);
        
#ifndef NDEBUG
        if (frames_read != input_frame_count) {
            log_debug("frames_read (%d) != input_frame_count (%d)\n",
                      (int)frames_read,
                      input_frame_count);
        }
        
        // This happens pretty much every time for the first buffer being
        // filled for each file
        if (src.output_frames_gen != src.output_frames) {
            log_debug("output_frames_gen (%d) != output_frames (%d)\n",
                      src.output_frames_gen,
                      src.output_frames);
        }
#endif
    }
    
    dec->frame_index += spec->frame_count;
}

void init_playback() {
    g_lock = create_mutex();
    open_wasapi_audio_stream(&audio_stream_callback, &g_decoder, &g_stream);
}

void unload_playing_file() {
    lock_mutex(g_lock);
    interrupt_audio_stream(&g_stream);
    close_decoder(&g_decoder);
    unlock_mutex(g_lock);
}

bool play_file(const wchar_t *path) {
    unload_playing_file();
    
    lock_mutex(g_lock);
    defer(unlock_mutex(g_lock));
    
    if (!open_decoder(&g_decoder, path)) {
        notify(NOTIFY_END_OF_TRACK);
        return false;
    }
    
    if (g_paused) set_playback_paused(false);
    
    wlog_debug(L"Opened file %s for playback\n", path);
    
    return true;
}

void set_playback_paused(bool value) {
    if (!g_decoder.file) return;
    if (g_paused != value) {
        g_paused = value;
        interrupt_audio_stream(&g_stream);
    }
}

void toggle_playback() {
    set_playback_paused(!g_paused);
}

Playback_State get_playback_state() {
    if (!g_decoder.file) return PLAYBACK_STATE_STOPPED;
    else if (g_paused) return PLAYBACK_STATE_PAUSED;
    else return PLAYBACK_STATE_PLAYING;
}

void set_playback_volume(float volume) {
    set_audio_stream_volume(&g_stream, volume);
}

float get_playback_volume() {
    return get_audio_stream_volume(&g_stream);
}

u64 get_playback_ms_duration() {
    SF_INFO info;
    u64 seconds;
    if (!g_decoder.file) return 0;
    info = g_decoder.info;
    seconds = info.frames/info.samplerate;
    return seconds * 1000;
}

i64 get_playback_ms_position() {
    if (!g_decoder.file) return 0;
    return (i64)(g_decoder.frame_index/g_stream.sample_rate)*1000;
}

void seek_playback_to_ms(i64 ms) {
    i64 frame;
    if (!g_decoder.file) return;
    lock_mutex(g_lock);
    frame = g_stream.sample_rate * (ms/1000);
    g_decoder.frame_index = sf_seek(g_decoder.file, frame, SEEK_SET);
    interrupt_audio_stream(&g_stream);
    unlock_mutex(g_lock);
    
    log_debug("Seek to frame %lld\n", frame);
}

float get_current_playback_peak() {
    return get_audio_stream_current_peak(&g_stream);
}

    