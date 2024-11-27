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
#include "array.h"
#include <sndfile.h>
#include <samplerate.h>
#include <math.h>
#include <string.h>

struct Buffer_View {
    i32 first_frame;
    i32 last_frame;
};

struct Decoder {
    SNDFILE *file;
    SRC_STATE *resampler;
    SF_INFO info;
    i64 frame_index;
    u64 buffer_first_frame;
    u64 buffer_timestamp;
    Array<float> buffer[MAX_AUDIO_CHANNELS];
    Array<float> prev_buffer[MAX_AUDIO_CHANNELS];
};

static Audio_Stream g_stream;
static Decoder g_decoder;
static Mutex g_lock;
static bool g_paused;

static void close_decoder(Decoder *dec) {
    if (dec->file) sf_close(dec->file);
    if (dec->resampler) src_delete(dec->resampler);
    for (i32 i = 0; i < g_stream.channel_count; ++i) {
        dec->buffer[i].free();
        dec->prev_buffer[i].free();
    }
    *dec = Decoder{};
}

static bool open_decoder(Decoder *dec, const wchar_t *filename) {
    close_decoder(dec);
    dec->file = sf_wchar_open(filename, SFM_READ, &dec->info);
    if (!dec->file) return false;
    
    return true;
}

static void extract_channel_samples(f32 *input, u32 frame_count, u32 channel_count, Array<float> *output) {
    u32 sample_count = frame_count*channel_count;
    
    for (u32 channel = 0; channel < channel_count; ++channel) {
        output[channel].clear();
        output[channel].push(frame_count);
    }
    
    for (u32 frame = 0; frame < frame_count; ++frame) {
        u32 first_sample = frame*channel_count;
        for (u32 channel = 0; channel < channel_count; ++channel) {
            output[channel][frame] = input[first_sample+channel];
        }
    }
}

static bool get_buffer_view(Decoder *dec, i32 frame_count, Buffer_View *view) {
    if (!dec->buffer[0].count) return false;
    
    i32 delta_ms = (i32)perf_time_to_millis(perf_time_now() - dec->buffer_timestamp);
    view->first_frame = delta_ms * ((u32)g_stream.sample_rate/1000);
    view->first_frame -= (i32)frame_count;
    view->first_frame = MAX(view->first_frame, 0);
    frame_count = MIN(frame_count, (i32)dec->buffer[0].count - view->first_frame);
    if (frame_count < 0) return false;
    view->last_frame = view->first_frame + frame_count-1;
    return true;
}

bool update_playback_buffer(Playback_Buffer *buffer) {
    Decoder *dec = &g_decoder;
    lock_mutex(g_lock);
    defer(unlock_mutex(g_lock));
    
    if (g_paused) {
        buffer->frame_count = 0;
        for (u32 i = 0; i < buffer->channel_count; ++i) {
            buffer->data[i].clear();
        }

        return true;
    }

    // We don't need to copy if the playback buffer hasn't changed
    if (buffer->data[0].data && buffer->timestamp == dec->buffer_timestamp) {
        return true;
    }
    
    buffer->timestamp = dec->buffer_timestamp;
    buffer->sample_rate = g_stream.sample_rate;
    buffer->frame_count = dec->buffer[0].count + dec->prev_buffer[0].count;
    buffer->channel_count = g_stream.channel_count;
    
    for (i32 i = 0; i < buffer->channel_count; ++i) {
        buffer->data[i].clear();
        dec->prev_buffer[i].copy_to(buffer->data[i]);
        dec->buffer[i].copy_to(buffer->data[i]);
    }
    
    return true;
}

bool get_playback_buffer_view(Playback_Buffer *buffer, i32 frame_count, Playback_Buffer_View *view) {
    if (!buffer->frame_count) {
        *view = Playback_Buffer_View{};
        return false;
    }
    
    i32 delta_ms = (i32)perf_time_to_millis(perf_time_now() - buffer->timestamp);
    i32 first_frame;
    first_frame = delta_ms * (buffer->sample_rate/1000);
    //first_frame -= frame_count;
    first_frame = MAX(first_frame, 0);
    frame_count = MIN(frame_count, buffer->frame_count - first_frame);
    if (frame_count < 0) return false;
    
    view->frame_count = frame_count;
    view->channel_count = buffer->channel_count;
    
    for (i32 i = 0; i < view->channel_count; ++i) {
        view->data[i] = &buffer->data[i].data[first_frame];
    }
    
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
    dec->buffer_first_frame = dec->frame_index;
    
    if (!needs_resampling) {
        sf_count_t frames_read = sf_readf_float(dec->file, output_buffer, spec->frame_count);
        if (frames_read < spec->frame_count) {
            notify(NOTIFY_NEXT_TRACK);
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
            notify(NOTIFY_NEXT_TRACK);
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
    
    bool have_prev_buffer = dec->buffer[0].count > 0;
    
    if (dec->buffer[0].count) for (i32 i = 0; i < spec->channel_count; ++i) {
        dec->prev_buffer[i].clear();
        dec->buffer[i].copy_to(dec->prev_buffer[i]);
    }
    
    extract_channel_samples(output_buffer, spec->frame_count, spec->channel_count, dec->buffer);
    
    if (have_prev_buffer) {
        f32 buffer_time_s = (f32)spec->frame_count/spec->sample_rate;
        u64 buffer_ticks = (u64)(buffer_time_s*perf_time_frequency());
        dec->buffer_timestamp = perf_time_now() - (buffer_ticks/2);
    }
    else {
        dec->buffer_timestamp = perf_time_now();
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
        notify(NOTIFY_NEXT_TRACK);
        return false;
    }
    
    if (g_paused) set_playback_paused(false);
    
    wlog_debug(L"Opened file %s for playback\n", path);
    
    return true;
}

void set_playback_paused(bool value) {
    lock_mutex(g_lock);
    defer(unlock_mutex(g_lock));

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

    