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
#include "os.h"
#include "audio.h"
#include "main.h"
#include "array.h"
#include "decoder.h"
#include <sndfile.h>
#include <samplerate.h>
#include <math.h>
#include <string.h>

struct Buffer_View {
    i32 first_frame;
    i32 last_frame;
};

struct Capture_Buffer {
    Array<float> next[MAX_AUDIO_CHANNELS];
    Array<float> prev[MAX_AUDIO_CHANNELS];
    u64 timestamp;
    u64 first_frame;
};

static Audio_Stream g_stream;
static Decoder g_decoder;
static Mutex g_lock;
static bool g_paused;
static Capture_Buffer g_capture;

static void deinterlace_buffer(f32 *input, u32 frames, u32 in_channels, u32 out_channels, Array<float> *output) {
    u32 sample = 0;
    u32 frame = 0;
    u32 sample_count = frames * in_channels;
    
    for (u32 ch = 0; ch < out_channels; ++ch) {
        output[ch].clear();
        output[ch].push(frames);
    }
    
    if (out_channels <= in_channels) {
        for (sample = 0; sample < sample_count; sample += in_channels) {
            for (u32 ch = 0; ch < out_channels; ++ch) {
                output[ch][frame] = input[sample+ch];
            }
            
            frame++;
        }
    }
    else {
        i32 remainder = out_channels - in_channels;
        
        for (sample = 0; sample < sample_count; sample += in_channels) {
            for (u32 ch = 0; ch < in_channels; ++ch) {
                output[ch][frame] = input[sample+ch];
            }
            
            for (i32 ch = 0; ch < remainder; ++ch) {
                output[ch+remainder][frame] = input[sample+ch];
            }
            
            frame++;
        }
    }
}

bool playback_update_capture_buffer(Playback_Buffer *buffer) {
    lock_mutex(g_lock);
    defer(unlock_mutex(g_lock));
    
    if (g_paused) {
        buffer->frame_count = 0;
        for (u32 i = 0; i < buffer->channels; ++i) {
            buffer->data[i].clear();
        }

        return true;
    }

    // We don't need to copy if the playback buffer hasn't changed
    if (buffer->data[0].data && buffer->timestamp == g_capture.timestamp) {
        return true;
    }
    
    buffer->channels = g_stream.channel_count;
    buffer->timestamp = g_capture.timestamp;
    buffer->sample_rate = g_stream.sample_rate;
    buffer->frame_count = g_capture.next[0].count + g_capture.prev[0].count;
    
    for (i32 i = 0; i < buffer->channels; ++i) {
        buffer->data[i].clear();
        g_capture.prev[i].copy_to(buffer->data[i]);
        g_capture.next[i].copy_to(buffer->data[i]);
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
    first_frame = MAX(first_frame, 0);
    frame_count = MIN(frame_count, buffer->frame_count - first_frame);
    if (frame_count < 0) return false;
    
    view->frame_count = frame_count;
    view->channels = buffer->channels;

    for (i32 i = 0; i < buffer->channels; ++i) {
        view->data[i] = &buffer->data[i].data[first_frame];
    }
    
    return true;
}

void audio_stream_callback(void *user_data, f32 *output_buffer, const Audio_Buffer_Spec *spec) {
    lock_mutex(g_lock);
    defer(unlock_mutex(g_lock));
    
    if (g_paused || !g_decoder.file) {
        zero_array(output_buffer, spec->channel_count * spec->frame_count);
        return;
    }
    
    Decoder *dec = (Decoder*)user_data;
    Decode_Status status = decoder_decode(dec, output_buffer, spec->frame_count, spec->channel_count, spec->sample_rate);
    if (status == DECODE_STATUS_EOF) notify(NOTIFY_REQUEST_NEXT_TRACK);
    
    i32 channels = g_stream.channel_count;

    if (g_capture.next[0].count > 0) {
        for (i32 i = 0; i < channels; ++i) {
            g_capture.prev[i].clear();
            g_capture.next[i].copy_to(g_capture.prev[i]);
        }
        
        deinterlace_buffer(
            output_buffer,
            spec->frame_count,
            spec->channel_count,
            channels,
            g_capture.next);
        
        f32 buffer_seconds = (f32)spec->frame_count/spec->sample_rate;
        u64 buffer_ticks = (u64)(buffer_seconds * perf_time_frequency());
        g_capture.timestamp = perf_time_now() - (buffer_ticks/2);
    }
    else {
        deinterlace_buffer(
            output_buffer,
            spec->frame_count,
            spec->channel_count,
            channels,
            g_capture.next);

        g_capture.timestamp = perf_time_now();
    }
}

void playback_init() {
    g_lock = create_mutex();
#ifdef _WIN32
    open_wasapi_audio_stream(&audio_stream_callback, &g_decoder, &g_stream);
#else
    open_portaudio_audio_stream(&audio_stream_callback, &g_decoder, &g_stream);
#endif
}

void playback_unload_file() {
    lock_mutex(g_lock);
    interrupt_audio_stream(&g_stream);
    decoder_close(&g_decoder);
    unlock_mutex(g_lock);
    for (u32 i = 0; i < MAX_AUDIO_CHANNELS; ++i) {
        g_capture.next[i].free();
        g_capture.prev[i].free();
    }
}

bool playback_load_file(const char *path) {
    playback_unload_file();
    
    lock_mutex(g_lock);
    defer(unlock_mutex(g_lock));
    
    if (!decoder_open(&g_decoder, path)) {
        notify(NOTIFY_REQUEST_NEXT_TRACK);
        return false;
    }
    
    if (g_paused) playback_set_paused(false);
    
    log_debug("Opened file %s for playback\n", path);
    
    return true;
}

void playback_set_paused(bool value) {
    lock_mutex(g_lock);
    defer(unlock_mutex(g_lock));

    if (!g_decoder.file) return;
    if (g_paused != value) {
        g_paused = value;
        interrupt_audio_stream(&g_stream);
        notify(NOTIFY_PLAYBACK_STATE_CHANGE);
    }
}

void playback_toggle() {
    playback_set_paused(!g_paused);
}

Playback_State playback_get_state() {
    if (!g_decoder.file) return PLAYBACK_STATE_STOPPED;
    else if (g_paused) return PLAYBACK_STATE_PAUSED;
    else return PLAYBACK_STATE_PLAYING;
}

void playback_set_volume(float volume) {
    set_audio_stream_volume(&g_stream, volume);
}

float playback_get_volume() {
    return get_audio_stream_volume(&g_stream);
}

int playback_get_bitrate() {
    return decoder_get_bitrate(&g_decoder);
}

void playback_get_file_info(Playback_File_Info *info) {
    if (!g_decoder.file) return;
    SF_FORMAT_INFO format;
    SF_INFO i = g_decoder.info;

    info->channels = i.channels;
    info->samplerate = i.samplerate;
    format.format = i.format & SF_FORMAT_TYPEMASK;
    sf_command(g_decoder.file, SFC_GET_FORMAT_INFO, &format, sizeof(format));
    info->format = format.name;
    format.format = i.format & SF_FORMAT_SUBMASK;
    sf_command(g_decoder.file, SFC_GET_FORMAT_INFO, &format, sizeof(format));
    info->codec = format.name;
}

u64 playback_get_duration_millis() {
    SF_INFO info;
    u64 seconds;
    if (!g_decoder.file) return 0;
    info = g_decoder.info;
    seconds = info.frames/info.samplerate;
    return seconds * 1000;
}

i64 playback_get_position_millis() {
    if (!g_decoder.file) return 0;
    return decoder_get_position_millis(&g_decoder);
}

void playback_seek_to_millis(i64 ms) {
    if (!g_decoder.file) return;
    lock_mutex(g_lock);
    decoder_seek_millis(&g_decoder, ms);
    interrupt_audio_stream(&g_stream);
    unlock_mutex(g_lock);
}

    