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
#include "metadata.h"
#include "filenames.h"
#include "video.h"
#include "array.h"
#include "taglib_file_name_workaround.h"
#include <tag_c.h>
#include <wchar.h>
#include <xxhash.h>

// @TODO: Make a more memory efficient way to store metadata
static Array<u32> g_filename_hashes;
static Array<Metadata> g_metadata;

Metadata_Index read_file_metadata(const wchar_t *path) {
    u32 filename_hash = hash_wstring(path);
    
    if (!g_metadata.count) {
        Metadata empty = {};
        empty.artist[0] = ' ';
        empty.album[0] = ' ';
        empty.title[0] = ' ';
        g_filename_hashes.append(0);
        g_metadata.append(empty);
    }
    
    i32 existing_index = linear_search(g_filename_hashes.data, g_filename_hashes.count, filename_hash);
    if (existing_index >= 0) return existing_index;
    
    char u8_path[PATH_LENGTH];
    wchar_to_utf8(path, u8_path, PATH_LENGTH);
    
    TagLib_File *file = taglib_file_new_wchar_(path);
    
    if (file) {
        defer(taglib_file_free(file));
        TagLib_Tag *tag = taglib_file_tag(file);
        Metadata metadata = {};
        u32 index;
        
        const TagLib_AudioProperties *props = taglib_file_audioproperties(file);
        
        if (props) {
            metadata.duration_seconds = taglib_audioproperties_length(props);
            format_time(metadata.duration_seconds, metadata.duration_string,
                        sizeof(metadata.duration_string));
        }
        
        if (tag) {
            char *title = taglib_tag_title(tag);
            char *artist = taglib_tag_artist(tag);
            char *album = taglib_tag_album(tag);
            
            if (title && title[0]) strncpy0(metadata.title, title, sizeof(metadata.title));
            else strncpy0(metadata.title, get_file_name(u8_path), sizeof(metadata.title));
            
            if (artist) strncpy0(metadata.artist, artist, sizeof(metadata.artist));
            if (album) strncpy0(metadata.album, album, sizeof(metadata.album));
            
            index = g_metadata.append(metadata);
            g_filename_hashes.append(filename_hash);
            return index;
        }
    }
    
    // If we fail to get the metadata, use empty strings for artist and album and just use the
    // file name as the title
    Metadata not_found = {};
    u32 index;
    not_found.artist[0] = ' ';
    not_found.album[0] = ' ';
    strncpy(not_found.title, get_file_name(u8_path), sizeof(not_found.title)-1);
    index = g_metadata.append(not_found);
    g_filename_hashes.append(filename_hash);
    return index;
}

bool update_file_metadata(Metadata_Index index, const wchar_t *path, Detailed_Metadata *new_md) {
    TagLib_File *file = taglib_file_new_wchar_(path);
    Metadata *old_md = &g_metadata[index];
    if (!file) return false;
    defer(taglib_file_free(file));

    TagLib_Tag *tag = taglib_file_tag(file);
    if (!tag) return false;
    
    taglib_tag_set_title(tag, new_md->title);
    taglib_tag_set_artist(tag, new_md->artist);
    taglib_tag_set_album(tag, new_md->album);
    taglib_tag_set_comment(tag, new_md->comment);
    taglib_tag_set_genre(tag, new_md->genre);
    taglib_tag_set_year(tag, new_md->year);
    taglib_tag_set_track(tag, new_md->track_number);

    strncpy0(old_md->title, new_md->title, sizeof(old_md->title));
    strncpy0(old_md->artist, new_md->artist, sizeof(old_md->artist));
    strncpy0(old_md->album, new_md->album, sizeof(old_md->album));

    return taglib_file_save(file);
}

bool read_detailed_file_metadata(const wchar_t *path, Detailed_Metadata *md, Image *cover) {
    TagLib_File *file = taglib_file_new_wchar_(path);
    
    if (cover) cover->data = NULL;
    
    if (file) {
        defer(taglib_file_free(file));
        TagLib_Tag *tag = taglib_file_tag(file);
        defer(if (tag) taglib_tag_free_strings());

        if (cover) {
            TagLib_Complex_Property_Attribute ***props = taglib_complex_property_get(file, "PICTURE");
            if (props) {
                TagLib_Complex_Property_Picture_Data pict;
                taglib_picture_from_complex_property(props, &pict);
                
                if (pict.data) load_image_from_memory(pict.data, pict.size, cover);
                
                taglib_complex_property_free(props);
            }
        }
        
        if (tag && md) {
            char *title = taglib_tag_title(tag);
            char *artist = taglib_tag_artist(tag);
            char *album = taglib_tag_album(tag);
            char *genre = taglib_tag_genre(tag);
            char *comment = taglib_tag_comment(tag);
            
            if (title) strncpy0(md->title, title, sizeof(md->title));
            if (artist) strncpy0(md->artist, artist, sizeof(md->artist));
            if (album) strncpy0(md->album, album, sizeof(md->album));
            if (genre) strncpy0(md->genre, genre, sizeof(md->genre));
            if (comment) strncpy0(md->comment, comment, sizeof(md->comment));
            md->year = taglib_tag_year(tag);
            md->track_number = taglib_tag_track(tag);
            
            return true;
        }
    }
    
    return false;
}

void retrieve_metadata(Metadata_Index index, Metadata *md) {
    *md = g_metadata[index];
}

#define METADATA_CACHE_MAGIC *(u32*)"MTDC"

static void write_u32(FILE *f, u32 value) {
    fwrite(&value, 4, 1, f);
}

static void write_u64(FILE *f, u64 value) {
    fwrite(&value, 8, 1, f);
}

static void read_u32(FILE *f, u32 *value) {
    if (value) fread(value, 4, 1, f);
    fseek(f, 4, SEEK_CUR);
}

static void read_u64(FILE *f, u64 *value) {
    if (value) fread(value, 8, 1, f);
    fseek(f, 8, SEEK_CUR);
}

static inline u32 mread_u32(void **memory) {
    u32 value;
    memcpy(&value, *memory, 4);
    *memory = (u8*)*memory + 4;
    return value;
}

static inline u64 mread_u64(void **memory) {
    u64 value;
    memcpy(&value, *memory, 8);
    *memory = (u8*)*memory + 8;
    return value;
}

void save_metadata_cache(const wchar_t *path) {
    FILE *f = _wfopen(path, L"wb");
    if (!f) return;
    defer(fclose(f));
    
    Array<char> sp = {};
    defer(sp.free());
    
    write_u32(f, METADATA_CACHE_MAGIC);
    write_u32(f, 0); // Version
    write_u32(f, 0); // Flags
    write_u32(f, g_metadata.count); // Track count
    
    for (u32 i = 0; i < g_metadata.count; ++i) {
        Metadata md = g_metadata[i];
        u32 title = sp.append_array(md.title, (u32)strlen(md.title)+1);
        u32 artist = sp.append_array(md.artist, (u32)strlen(md.artist)+1);
        u32 album = sp.append_array(md.album, (u32)strlen(md.album)+1);
        
        write_u32(f, g_filename_hashes[i]);
        write_u32(f, title);
        write_u32(f, artist);
        write_u32(f, album);
        write_u32(f, md.duration_seconds);
    }
    
    fwrite(sp.data, 1, sp.count, f);
}

void load_metadata_cache(const wchar_t *path) {
    FILE *f = _wfopen(path, L"rb");
    if (!f) return;
    defer(fclose(f));
    START_TIMER(load_metadata, "Load metadata");
    
    fseek(f, 0, SEEK_END);
    i64 size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    void *file_buffer = malloc(size);
    void *data = file_buffer;
    defer(free(file_buffer));
    
    fread(data, size, 1, f);
    
    u32 magic = mread_u32(&data);
    u32 version = mread_u32(&data);
    u32 flags = mread_u32(&data);
    u32 file_count = mread_u32(&data);
    
    // 16 byte header + 20 bytes per track
    const char *string_pool = (char*)file_buffer + (16 + (file_count * 20));
    
    for (u32 i = 0; i < file_count; ++i) {
        Metadata md = {};
        u32 hash = mread_u32(&data);
        u32 title = mread_u32(&data);
        u32 artist = mread_u32(&data);
        u32 album = mread_u32(&data);
        u32 duration = mread_u32(&data);
        
        strncpy0(md.title, &string_pool[title], sizeof(md.title));
        strncpy0(md.artist, &string_pool[artist], sizeof(md.artist));
        strncpy0(md.album, &string_pool[album], sizeof(md.album));
        md.duration_seconds = duration;
        format_time(md.duration_seconds, md.duration_string,
                    sizeof(md.duration_string));
        
        g_filename_hashes.append(hash);
        g_metadata.append(md);
    }
    
    STOP_TIMER(load_metadata);
    log_info("Loaded %u files from metadata cache\n", file_count);
}
                                              
                                              