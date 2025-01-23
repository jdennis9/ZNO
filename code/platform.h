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

void platform_init();
void platform_set_window_title(const char *title);
void platform_poll_events();

#endif
