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
#include "filenames.h"
#include "array.h"
#include <xxhash.h>

static u32 push_string(Array<char>& pool, const char *string, u32 length = 0) {
    if (length == 0) length = (u32)strlen(string);
    u32 offset = pool.push(length+1);
    memcpy(&pool[offset], string, length);
    pool[offset+length] = 0;
    return offset;
}

static i32 lookup_path(const Path_Pool& pool, u32 hash) {
    for (u32 i = 0; i < pool.files.count; ++i) {
        if (pool.files[i].hash == hash) return i;
    }
    
    return -1;
}

Path_Index store_file_path(Path_Pool& pool, const char *full_path) {
    u32 full_hash = hash_string(full_path);
    
    {
        i32 index = lookup_path(pool, full_hash);
        if (index >= 0)
            return index;
    }
    
    const char *filename = get_file_name(full_path);
    ptrdiff_t base_path_length = filename - full_path;
    ASSERT(base_path_length >= 0);
    u32 folder_hash = XXH32(full_path, base_path_length, 0);
    i32 folder_index = -1;
    
    for (u32 i = 0; i < pool.folders.count; ++i) {
        if (pool.folders[i].hash == folder_hash) {
            folder_index = i;
            break;
        }
    }
    
    if (folder_index < 0) {
        Folder_Entry folder = {};
        folder.name = push_string(pool.string_pool, full_path, (u32)base_path_length);
        folder.hash = folder_hash;
        folder_index = pool.folders.append(folder);
    }
    
    File_Entry file = {};
    file.hash = full_hash;
    file.name = push_string(pool.string_pool, filename);
    file.folder_index = folder_index;
    pool.folders[file.folder_index].file_count++;
    
    return pool.files.append(file);
}

Path_Index store_file_path(Path_Pool& pool, const wchar_t *path) {
    char buffer[PATH_LENGTH];
    wchar_to_utf8(path, buffer, PATH_LENGTH);
    return store_file_path(pool, buffer);
}

void retrieve_file_path(const Path_Pool& pool, Path_Index index, char *buffer, u32 buffer_size) {
    File_Entry file = pool.files[index];
    Folder_Entry folder = pool.folders[file.folder_index];
    memset(buffer, 0, buffer_size);
    snprintf(buffer, buffer_size, "%s%s", &pool.string_pool[folder.name], &pool.string_pool[file.name]);
}

void retrieve_file_path(const Path_Pool& pool, Path_Index index, wchar_t *buffer, u32 buffer_size) {
    char utf8[PATH_LENGTH];
    retrieve_file_path(pool, index, utf8, PATH_LENGTH);
    utf8_to_wchar(utf8, buffer, buffer_size);
}
    
    