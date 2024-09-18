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
#ifndef METADATA_H
#define METADATA_H

#include "defines.h"

typedef u32 Metadata_Index;

struct Image;

// Typically needed metadata
struct Metadata {
    char album[64];
    char artist[64];
    char title[128];
    // The duration_string is automatically formatted
    // when the metadata is loaded
    char duration_string[60];
    u32 duration_seconds;
};

struct Detailed_Metadata {
    u32 track_number;
    u32 year;
    char comment[1016];
    char title[128];
    char album[64];
    char artist[64];
    char genre[64];
};

Metadata_Index read_file_metadata(const wchar_t *path);
bool read_detailed_file_metadata(const wchar_t *path, Detailed_Metadata *md, Image *image = NULL);
void retrieve_metadata(Metadata_Index index, Metadata *md);
void save_metadata_cache(const wchar_t *path);
void load_metadata_cache(const wchar_t *path);

#endif //METADATA_H

