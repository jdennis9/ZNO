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
#ifndef FILENAMES_H
#define FILENAMES_H

#include "defines.h"
#include "array.h"
#include <string.h>

typedef u32 Path_Index;

struct Folder_Entry {
    u32 hash;
    u32 name;
    u32 file_count;
};

struct File_Entry {
    u32 hash;
    u32 name;
    u32 folder_index;
};

// Memory effecient way of storing file paths.
// Never stores a folder string twice.
struct Path_Pool {
    Array<Folder_Entry> folders;
    Array<File_Entry> files;
    Array<char> string_pool;
};

Path_Index store_file_path(Path_Pool& pool, const char *path);
Path_Index store_file_path(Path_Pool& pool, const wchar_t *path);
void retrieve_file_path(const Path_Pool& pool, Path_Index index, char *buffer, u32 buffer_size);
void retrieve_file_path(const Path_Pool& pool, Path_Index index, wchar_t *buffer, u32 buffer_size);

static inline const char *get_file_extension(const char *path) {
    i64 length = strlen(path);
    for (i64 i = length - 1; i >= 0; i--) {
        if (path[i] == '.') {
            return &path[i + 1];
        }
    }
    
    return path;
}

static const char *get_file_name(const char *path) {
    i64 length = strlen(path);
    
    for (i64 i = length - 1; i >= 0; --i) {
        if (path[i] == '\\') return &path[i + 1];
    }
    
    return path;
}

static u32 get_file_name_length_without_extension(const char *path) {
    const char *filename = get_file_name(path);
    const char *extension = get_file_extension(path);
    
    if (extension == path || extension < filename) return (u32)strlen(filename);
    else return (u32)(extension - filename - 1);
}

#endif //FILENAMES_H

