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

enum Playback_State{
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

void init_playback();
bool play_file(const wchar_t *path);
void unload_playing_file();
void set_playback_paused(bool paused);
void toggle_playback();
Playback_State get_playback_state();
float get_playback_volume();
void set_playback_volume(float volume);
// In milliseconds
u64 get_playback_ms_duration();
i64 get_playback_ms_position();
void seek_playback_to_ms(i64 ms);
// Get the volume peak at the current time (0-1)
float get_current_playback_peak();
// Copy the global audio buffer if the timestamp doesn't match the timestamp of
// the provided buffer
bool update_playback_buffer(Playback_Buffer *buffer);
// Get a view of the playback buffer going 
// frame_count frames back from the current position
bool get_playback_buffer_view(Playback_Buffer *buffer, i32 frame_count, Playback_Buffer_View *view);

#endif //PLAYBACK_H

