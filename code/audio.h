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
#ifndef AUDIO_H
#define AUDIO_H

// Facilities for streaming audio

#include "defines.h"

#define MAX_AUDIO_CHANNELS 2

struct Audio_Buffer_Spec {
    i32 frame_count;
    i32 channel_count;
    i32 sample_rate;
};

typedef void Fill_Audio_Buffer_Callback(void *data, f32 *buffer, const Audio_Buffer_Spec *spec);
typedef void Audio_Stream_Interrupt_Fn(void *data);
typedef void Audio_Stream_Close_Fn(void *data);
typedef void Audio_Stream_Set_Volume_Fn(void *data, float volume);
typedef float Audio_Stream_Get_Volume_Fn(void *data);

struct Audio_Stream {
    void *data;
    i32 sample_rate;
    i32 channel_count;
    i32 latency_ms;
    i32 buffer_duration_ms;
    
    Audio_Stream_Interrupt_Fn *interrupt_fn;
    Audio_Stream_Set_Volume_Fn *set_volume_fn;
    Audio_Stream_Get_Volume_Fn *get_volume_fn;
    Audio_Stream_Close_Fn *close_fn;
};

// audio_impl_wasapi.cpp
bool open_wasapi_audio_stream(Fill_Audio_Buffer_Callback *callback, void *callback_data, Audio_Stream *stream);

static inline void interrupt_audio_stream(Audio_Stream *stream) {
    if (stream->interrupt_fn) stream->interrupt_fn(stream->data);
}

static inline void set_audio_stream_volume(Audio_Stream *stream, f32 volume) {
    if (stream->set_volume_fn) stream->set_volume_fn(stream->data, volume);
}

static inline float get_audio_stream_volume(Audio_Stream *stream) {
    if (stream->get_volume_fn) return stream->get_volume_fn(stream->data);
    return 1.f;
}

static inline void close_audio_stream(Audio_Stream *stream) {
    if (stream->close_fn) stream->close_fn(stream->data);
}

#endif //AUDIO_H

