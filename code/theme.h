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
#ifndef THEME_H
#define THEME_H

#include "defines.h"

#define MAX_THEME_NAME_LENGTH 127

enum Theme_Color {
    THEME_COLOR_PLAYING_INDICATOR,
    THEME_COLOR_PLAYING_TEXT,
    THEME_COLOR_PEAK_METER,
    THEME_COLOR_PEAK_METER_BG,
    THEME_COLOR__COUNT,
};

void set_default_theme();
void load_theme(const char *name);
void save_theme(const char *name);
// Returns true if there are unsaved theme changes
bool show_theme_editor_gui();
const char *show_theme_selector_gui();
const char *get_loaded_theme();
u32 get_theme_color(Theme_Color color);

#endif //THEME_H
