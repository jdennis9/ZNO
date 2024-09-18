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

enum Playback_State{
    PLAYBACK_STATE_STOPPED,
    PLAYBACK_STATE_PAUSED,
    PLAYBACK_STATE_PLAYING,
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

#endif //PLAYBACK_H

