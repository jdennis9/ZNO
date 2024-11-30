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
#include <ctype.h>
#include "library.h"
#include "filenames.h"
#include "array.h"

struct Library {
    Array<Path_Index> paths;
    Array<Metadata_Index> metadata;
};

static Library g_library;

static inline bool case_insensitive_string_equal(const wchar_t *a, const wchar_t *b) {
    for (; *a && *b; ++a, ++b) {
        if (tolower(*a) != tolower(*b)) return false;
    }
    return !*a && !*b;
}

// Guess from file extension whether a file is supported
// @NOTE: This needs to be maintained within platform.cpp (AUDIO_FILE_TYPES)
static bool is_supported_file(const wchar_t *path) {
    const wchar_t *extension = wcsrchr((wchar_t*)path, '.');
    if (!extension) return false;

    return 
        case_insensitive_string_equal(extension, L".mp3") ||
        case_insensitive_string_equal(extension, L".aiff") ||
        case_insensitive_string_equal(extension, L".flac") ||
        case_insensitive_string_equal(extension, L".opus") ||
        case_insensitive_string_equal(extension, L".ape") ||
        case_insensitive_string_equal(extension, L".wav");
}

Track library_add_track(const wchar_t *path) {
    if (!is_supported_file(path)) return 0;

    Path_Index path_index = store_file_path(path);
    Metadata_Index md_index = read_file_metadata(path);

    u32 index = g_library.paths.append(path_index);
    g_library.metadata.append(md_index);

    return index + 1;
}

void library_get_track_metadata(Track track, Metadata *md) {
    ASSERT(track != 0);
    u32 md_index = g_library.metadata[track-1];
    retrieve_metadata(md_index, md);
}

// Buffer must be at least PATH_LENGTH characters
void library_get_track_path(Track track, wchar_t *buffer) {
    ASSERT(track != 0);
    u32 path_index = g_library.paths[track-1];
    retrieve_file_path(path_index, buffer, PATH_LENGTH);
}

void library_get_track_path(Track track, char *buffer) {
    ASSERT(track != 0);
    u32 path_index = g_library.paths[track-1];
    retrieve_file_path(path_index, buffer, PATH_LENGTH);
}
