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
#ifndef PLAYBACK_H
#define PLAYBACK_H

#include "defines.h"
#include "audio.h"
#include "array.h"

enum Playback_State {
    PLAYBACK_STATE_STOPPED,
    PLAYBACK_STATE_PAUSED,
    PLAYBACK_STATE_PLAYING,
};

struct Playback_Buffer {
    Array<float> data[MAX_AUDIO_CHANNELS];
    u64 timestamp;
    i32 frame_count;
    i32 channel_count;
    i32 sample_rate;
};

struct Playback_Buffer_View {
    const f32 *data[MAX_AUDIO_CHANNELS];
    i32 frame_count;
    i32 channel_count;
};

void playback_init();
bool playback_load_file(const wchar_t *path);
void playback_unload_file();
void playback_set_paused(bool paused);
void playback_toggle();
Playback_State playback_get_state();
float playback_get_volume();
void playback_set_volume(float volume);
u64 playback_get_duration_millis();
i64 playback_get_position_millis();
void playback_seek_to_millis(i64 ms);
// Copy the global audio buffer if the timestamp doesn't match the timestamp of
// the provided buffer
bool playback_update_capture_buffer(Playback_Buffer *buffer);
// Get a view of the playback buffer going 
// frame_count frames back from the current position
bool get_playback_buffer_view(Playback_Buffer *buffer, i32 frame_count, Playback_Buffer_View *view);

#endif //PLAYBACK_H

