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
#ifndef LIBRARY_H
#define LIBRARY_H
#include "defines.h"
#include "metadata.h"

// 0 means invalid track
typedef u32 Track;

Track library_add_track(const wchar_t *path);
void library_get_track_metadata(Track track, Metadata *md);
// Buffer must be at least PATH_LENGTH characters
void library_get_track_path(Track track, wchar_t *buffer);
void library_get_track_path(Track track, char *buffer);

#endif
