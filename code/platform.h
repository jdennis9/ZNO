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
#ifndef PLATFORM_H
#define PLATFORM_H

#include "defines.h"

extern char PLATFORM_METADATA_PATH[PATH_LENGTH];
extern char PLATFORM_PLAYLIST_PATH[PATH_LENGTH];
extern char PLATFORM_PREFS_PATH[PATH_LENGTH];

bool platform_init();
void platform_deinit();
void platform_init_imgui();
void platform_show_window(bool show);
void platform_resize_window(int width, int height);
void platform_get_window_size(int *width, int *height);
void platform_set_window_title(const char *title);
float platform_get_dpi_scale();
// Returns false when a quit is requested
bool platform_poll_events();
void platform_notify(int message);
void platform_apply_preferences();

#ifdef _WIN32
void platform_windows_show_hotkey_editor();
#endif

#endif
