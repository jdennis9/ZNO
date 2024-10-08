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
#ifndef PLAYLIST_H
#define PLAYLIST_H

#include "defines.h"
#include "array.h"
#include "filenames.h"
#include "platform.h"
#include "metadata.h"
#include <stdlib.h>
#include <xxhash.h>
#include <ctype.h>

#define PLAYLIST_NAME_MAX 128

struct Playlist;

enum Sort_Metric {
    SORT_METRIC_NONE,
    SORT_METRIC_ALBUM,
    SORT_METRIC_ARTIST,
    SORT_METRIC_TITLE,
    SORT_METRIC_DURATION,
    SORT_METRIC__LAST = SORT_METRIC_TITLE,
};

enum {
    SORT_ORDER_ASCENDING,
    SORT_ORDER_DESCENDING,
};

#define PLAYLIST_SORT_METRIC__COUNT (SORT_METRIC__LAST+1)

struct Track {
    Path_Index path;
    Metadata_Index metadata;
    
    INLINE bool is_null() const {
        return metadata == 0;
    }
    
    INLINE bool operator==(const Track& other) const {
        return other.metadata == metadata && other.path == path;
    }
    
    INLINE bool operator!=(const Track& other) const {
        return other.metadata != metadata || other.path != path;
    }
};

static inline const char *sort_metric_to_string(int metric) {
    switch (metric) {
        case SORT_METRIC_ALBUM: return "ALBUM";
        case SORT_METRIC_ARTIST: return "ARTIST";
        case SORT_METRIC_TITLE: return "TITLE";
        case SORT_METRIC_DURATION: return "DURATION";
        default: return "NONE";
    }
}

static inline const char *sort_order_to_string(int order) {
    switch (order) {
        case SORT_ORDER_DESCENDING: return "DESCENDING";
        default: return "ASCENDING";
    }
}

static inline int sort_metric_from_string(const char *string) {
    for (u32 i = 0; i < PLAYLIST_SORT_METRIC__COUNT; ++i) {
        if (!strcmp(sort_metric_to_string(i), string)) {
            return i;
        }
    }
    
    return SORT_METRIC_NONE;
}

static inline int sort_order_from_string(const char *string) {
    if (!strcmp(string, sort_order_to_string(SORT_ORDER_DESCENDING)))
        return SORT_ORDER_DESCENDING;
    return SORT_ORDER_ASCENDING;
}

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

static inline bool track_from_file(const wchar_t *path, Track *track) {
    if (!is_supported_file(path)) {
        wlog_debug(L"File %s is unsupported\n", path);
        return false;
    }
    // This slows load times by more that 10x. Need to do something about it
    /*if (!does_file_exist(path)) {
        wlog_debug(L"File %s is does not exist\n", path);
        return false;
    }*/
    track->path = store_file_path(path);
    track->metadata = read_file_metadata(path);
    return true;
}

struct Playlist;

void sort_playlist(Playlist& playlist, int metric, int order = SORT_ORDER_ASCENDING);
void sort_playlist_array(Array<Playlist>& playlists, int metric, int order = SORT_ORDER_ASCENDING);

struct Playlist {
    // Can be empty. Currently only used for albums
    char creator[PLAYLIST_NAME_MAX];
    // Used to get the playlist id. Must not be empty
    char name[PLAYLIST_NAME_MAX];
    // DO NOT CALL APPEND DIRECTLY ON THIS IT WILL MESS UP SORTING
    Array<Track> tracks;
    int sort_metric;
    int sort_order;
    bool unsorted;
    
    inline u32 get_id() const {
        ASSERT(name[0]);
        return hash_string(name);
    }
    
    inline i32 index_of_track(const Track& track) const {
        return linear_search<Track>(tracks.data, tracks.count, track);
    }
    
    inline void set_name(const char *new_name) {
        memset(name, 0, PLAYLIST_NAME_MAX);
        strncpy0(name, new_name, PLAYLIST_NAME_MAX);
    }
    
    inline void shuffle() {
        u32 count = tracks.count;
        for (u32 i = 0; i < count; ++i) {
            u32 s = rand() % count;
            swap(tracks[i], tracks[s]);
        }
        unsorted = true;
    }
    
    
    inline bool add_track(const Track& track) {
        bool added = tracks.append_unique(track);
        return added;
    }
    
    inline bool add_track(const wchar_t *path) {
        Track track = {};
        if (track_from_file(path, &track)) {
            return this->add_track(track, no_sort);
        }
        return false;
    }
    
    inline void add_tracks(const Array<Track>& t) {
        for (u32 i = 0; i < t.count; ++i) {
            this->add_track(t[i]);
        }
    }
    
    inline void sort() {
        sort_playlist(*this, sort_metric, sort_order);
    }
    
    inline void clear() {
        tracks.clear();
    }
    
    inline void copy_to(Playlist& other) const {
        for (u32 i = 0; i < tracks.count; ++i) {
            other.add_track(tracks[i]);
        }
    }
    
    i32 repeat(i32 index) {
        i32 count = (i32)tracks.count;
        if (index < 0) index = count + index;
        if (index >= count) return index - ((index/count)*count);
        return index;
    }
};

#endif //PLAYLIST_H
