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
#include "playlist.h"
#include "util.h"
#include <string.h>
#include <xxhash.h>
#include <stdlib.h>

typedef int Compare_Fn(const void*, const void*);

static inline int strcasecmp(const char *a, const char *b) {
    int ca, cb;
    do {
        ca = *(u8*)a;
        cb = *(u8*)b;
        ca = to_lower(to_upper(ca));
        cb = to_lower(to_upper(cb));
        a++;
        b++;
    } while ((ca == cb) && ca && cb);
    
    return ca - cb;
}

static int compare_titles_descending(const void *p_a, const void *p_b) {
    Track a = *(Track*)p_a;
    Track b = *(Track*)p_b;
    Metadata am;
    Metadata bm;
    
    library_get_track_metadata(a, &am);
    library_get_track_metadata(b, &bm);
    
    return strcasecmp(am.title, bm.title);
}

static int compare_artists_descending(const void *p_a, const void *p_b) {
    Track a = *(Track*)p_a;
    Track b = *(Track*)p_b;
    Metadata am;
    Metadata bm;
    
    library_get_track_metadata(a, &am);
    library_get_track_metadata(b, &bm);
    
    int cmp = strcasecmp(am.artist, bm.artist);
    if (!cmp) cmp = strcasecmp(am.album, bm.album);
    if (!cmp) cmp = strcasecmp(am.title, bm.title);
    return cmp;
}

static int compare_albums_descending(const void *p_a, const void *p_b) {
    Track a = *(Track*)p_a;
    Track b = *(Track*)p_b;
    Metadata am;
    Metadata bm;
    
    library_get_track_metadata(a, &am);
    library_get_track_metadata(b, &bm);
    
    int cmp = strcasecmp(am.album, bm.album);
    if (!cmp) cmp = strcasecmp(am.title, bm.title);
    return cmp;
}

static int compare_duration_descending(const void *p_a, const void *p_b) {
    Track a = *(Track*)p_a;
    Track b = *(Track*)p_b;
    Metadata am;
    Metadata bm;
    
    library_get_track_metadata(a, &am);
    library_get_track_metadata(b, &bm);
    
    if (am.duration_seconds == bm.duration_seconds) return 0;
    else if (am.duration_seconds < bm.duration_seconds) return -1;
    return 1;
}

static int compare_titles_ascending(const void *p_a, const void *p_b) {
    int cmp = compare_titles_descending(p_a, p_b);
    if (cmp == 0) return 0;
    return -cmp;
}

static int compare_artists_ascending(const void *p_a, const void *p_b) {
    int cmp = compare_artists_descending(p_a, p_b);
    if (cmp == 0) return 0;
    return -cmp;
}

static int compare_albums_ascending(const void *p_a, const void *p_b) {
    int cmp = compare_albums_descending(p_a, p_b);
    if (cmp == 0) return 0;
    return -cmp;
}

static int compare_duration_ascending(const void *p_a, const void *p_b) {
    int cmp = compare_duration_descending(p_a, p_b);
    if (cmp == 0) return 0;
    return -cmp;
}

static Compare_Fn *get_compare_fn_from_metric_and_order(int metric, int order) {
    if (metric == SORT_METRIC_TITLE && order == SORT_ORDER_DESCENDING)
        return &compare_titles_descending;
    if (metric == SORT_METRIC_ARTIST && order == SORT_ORDER_DESCENDING)
        return &compare_artists_descending;
    if (metric == SORT_METRIC_ALBUM && order == SORT_ORDER_DESCENDING)
        return &compare_albums_descending;
    if (metric == SORT_METRIC_DURATION && order == SORT_ORDER_DESCENDING)
        return &compare_duration_descending;
    
    if (metric == SORT_METRIC_TITLE && order == SORT_ORDER_ASCENDING)
        return &compare_titles_ascending;
    if (metric == SORT_METRIC_ARTIST && order == SORT_ORDER_ASCENDING)
        return &compare_artists_ascending;
    if (metric == SORT_METRIC_ALBUM && order == SORT_ORDER_ASCENDING)
        return &compare_albums_ascending;
    if (metric == SORT_METRIC_DURATION && order == SORT_ORDER_ASCENDING)
        return &compare_duration_ascending;
    
    return NULL;
}

void sort_playlist(Playlist& playlist, int metric, int order) {
    Compare_Fn *compare_fn = get_compare_fn_from_metric_and_order(metric, order);
    if (!compare_fn) return;
    qsort(playlist.tracks.data, playlist.tracks.count, sizeof(Track), compare_fn);
    
    playlist.sort_metric = metric;
    playlist.sort_order = order;
    playlist.unsorted = false;
}

u32 playlist_remove_missing_tracks(Playlist& playlist) {
    u32 count = 0;
    for (i32 i = 0; i < (i32)playlist.tracks.count; ++i) {
        Track track = playlist.tracks[i];
        char path[PATH_LENGTH];
        library_get_track_path(track, path);

        if (!does_file_exist(path)) {
            playlist.tracks.ordered_remove(i);
            i--;
            count++;
        }
    }

    return count;
}
