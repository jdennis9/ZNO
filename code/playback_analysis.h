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
#ifndef PLAYBACK_ANALYSIS_H
#define PLAYBACK_ANALYSIS_H

#include "defines.h"

void update_playback_analyzers(f32 delta_ms);
f32 get_playback_peak();
// Show the spectrogram as an ImGui histogram
void show_spectrum_widget(const char *str_id, float width = 0.f);
// Show the spectrogram in a window, occupying the whole window
void show_spectrum_ui();

#endif //PLAYBACK_ANALYSIS_H
