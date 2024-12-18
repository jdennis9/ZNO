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
#ifndef MAIN_H
#define MAIN_H

#include "defines.h"
#include "array.h"
#include <ini.h>
#include <string.h>
#include <stdlib.h>

#define APP_VERSION_STRING "0.5.2"

// Functions for communicating with main

enum {
    NOTIFY_QUIT,
    NOTIFY_REQUEST_NEXT_TRACK,
    NOTIFY_MINIMIZE_TO_TRAY,
    NOTIFY_REQUEST_PAUSE,
    NOTIFY_REQUEST_PLAY,
    NOTIFY_REQUEST_PREV_TRACK,
    NOTIFY_NEW_TRACK_PLAYING,
    NOTIFY_PLAYBACK_STATE_CHANGE,
    NOTIFY_BRING_WINDOW_TO_FOREGROUND,
};

enum {
    HOTKEY_PREV_TRACK,
    HOTKEY_NEXT_TRACK,
    HOTKEY_TOGGLE_PLAYBACK,
    HOTKEY__LAST = HOTKEY_TOGGLE_PLAYBACK,
};

#define HOTKEY__COUNT (HOTKEY__LAST+1)

struct Preferences;

struct File_Drag_Drop_Payload {
    Array<wchar_t> string_pool;
    // Offsets of paths into the string pool
    Array<u32> offsets;
};

void notify(int message);
void capture_next_input_and_bind_to_hotkey(int hotkey);
f32 get_dpi_scale();
void resize_main_window(int width, int height);

const File_Drag_Drop_Payload& get_file_drag_drop_payload();
bool is_hotkey_being_captured(int hotkey);
// Returns true if the hotkey is bound. If the hotkey is not bound, the buffer is unchanged
bool get_hotkey_bind_string(int hotkey, char *buffer, int buffer_size);
const char *get_hotkey_name(int hotkey);
// Get main preferences
Preferences& get_preferences();
// Notify main that we have made a change to
// the preferences and they need to be saved
void set_preferences_dirty();
// Applies changes made to main preferences
void apply_preferences();

void set_window_title_message(const char *format, ...);

#endif //MAIN_H

