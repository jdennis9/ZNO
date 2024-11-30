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
#include "ui.h"
#include "ui_functions.h"
#include "array.h"
#include "playlist.h"
#include "playback.h"
#include "playback_analysis.h"
#include "preferences.h"
#include "metadata.h"
#include "main.h"
#include "video.h"
#include "layouts.h"
#include "theme.h"
#include "filenames.h"
#include "platform.h"
#include <ini.h>
#include <imgui.h>
#include <atomic>

#define PLAYLIST_DIRECTORY "Playlists"
#define PLAYLIST_DIRECTORY_W L"Playlists"
#define DATA_DIRECTORY "Data"
#define DATA_DIRECTORY_W L"Data"
#define LIBRARY_PATH DATA_DIRECTORY_W "\\Library.txt"
#define QUEUE_PATH DATA_DIRECTORY_W "\\Queue.txt"
#define STATE_PATH DATA_DIRECTORY "\\State.ini"

static const wchar_t *REQUIRED_DIRECTORIES[] = {
    PLAYLIST_DIRECTORY_W,
    DATA_DIRECTORY_W,
};

static const char *SHUFFLE_ICON = u8"\xf074";
static const char *PREV_TRACK_ICON = u8"\xf048";
static const char *NEXT_TRACK_ICON = u8"\xf051";
static const char *PLAY_ICON = u8"\xf04b";
static const char *PAUSE_ICON = u8"\xf04c";

typedef void Window_Show_Fn();

struct Filter_Properties {
    char query[128];
    Array<Track> input;
};

struct UI_State {
    Array<Playlist> user_playlists;
    Array<Path_Index> user_playlist_paths;
    
    Array<u32> album_ids;
    Array<Playlist> albums;
    u32 viewing_album_id;
    
    Filter_Properties filter;
    ImGuiID filter_popup_id;
    
    Playlist library;
    Playlist search_results;
    Playlist queue;
    u32 library_id;
    u32 search_results_id;
    u32 queue_id;
    
    Detailed_Metadata detailed_metadata;
    Track detailed_metadata_track;
    Texture *cover_art;
    
    i32 queue_position;
    Track current_track;
    u32 current_playlist_id;
    u32 selected_user_playlist_id;
    
    u32 track_selection_playlist_id;
    Array<Track> track_selection;
    
    bool show_window[WINDOW__COUNT];
    bool focus_window[WINDOW__COUNT];
    bool is_window_hovered[WINDOW__COUNT];
    ImGuiWindowFlags window_flags[WINDOW__COUNT];
    Window_Show_Fn *window_show_fn[WINDOW__COUNT];
    
    bool want_to_create_playlist_from_selection;
    
    bool ready;
    bool library_altered;
    bool shuffle_on;
    bool show_prefs;
    bool show_hotkeys;
    bool show_about_window;
    
    // Super small font, currently used by spectrum analyzer to
    // denote band frequencies
    ImFontAtlas mini_font_atlas;
    ImFont *mini_font;

    Track metadata_editor_track;

    Thread track_scan_thread;
    struct {
        std::atomic<u32> total_track_count;
        std::atomic<u32> tracks_loaded;
        std::atomic<u32> errors;
        std::atomic<u32> done;
    } track_scan_progress;
    struct {
        Array<wchar_t> path_pool;
        Array<u32> paths;
    } track_scan_buffer;
};

static UI_State ui;

static void show_prefs_editor();
static void show_hotkey_editor();
static void update_detailed_metadata();
static void save_all_state();
static void show_about();

static void add_to_albums(const Track& track) {
    Metadata track_md;
    library_get_track_metadata(track, &track_md);
    // If there is no album tag we don't need to do anything
    if (!track_md.album[0]) return;
    
    u32 album_id = hash_string(track_md.album);
    i32 album_index = ui.album_ids.lookup(album_id);
    
    if (album_index < 0) {
        log_debug("Add album %s\n", track_md.album);
        
        Playlist playlist = {};
        playlist.set_name(track_md.album);
        strncpy0(playlist.creator, track_md.artist, sizeof(playlist.creator));
        
        ui.album_ids.append(album_id);
        album_index = ui.albums.append(playlist);
        ui.albums[album_index].add_track(track);
        return;
    }
    
    Playlist& album = ui.albums[album_index];
    if (strcmp(album.creator, track_md.artist)) {
        zero_array(album.creator, ARRAY_LENGTH(album.creator));
        strcpy(album.creator, "Various Artists");
    }
    album.tracks.append_unique(track);
}

Recurse_Command add_tracks_to_async_scan(void *in_data, const wchar_t *path, bool is_folder) {
    if (is_folder) {
        for_each_file_in_folder(path, &add_tracks_to_async_scan, NULL);
    }
    else {
        size_t len = (u32)wcslen(path);
        size_t offset = ui.track_scan_buffer.path_pool.push(len+1);
        memcpy(&ui.track_scan_buffer.path_pool[offset], path, len * sizeof(wchar_t));
        ui.track_scan_buffer.path_pool[offset + len] = 0;
        ui.track_scan_buffer.paths.append(offset);
    }

    return RECURSE_CONTINUE;
}

const char *get_window_name(int window) {
    ASSERT(window >= 0 && window < WINDOW__COUNT);
    
    switch (window) {
        case WINDOW_LIBRARY: return "Library";
        case WINDOW_QUEUE: return "Queue";
        case WINDOW_ALBUM_LIST: return "Album List";
        case WINDOW_SEARCH_RESULTS: return "Search Results";
        case WINDOW_METADATA: return "Metadata";
        case WINDOW_USER_PLAYLISTS: return "Your Playlists";
        case WINDOW_PLAYLIST_TRACKS: return "Playlist";
        case WINDOW_THEME_EDITOR: return "Theme";
        case WINDOW_METADATA_EDITOR: return "Edit Metadata";
        case WINDOW_V_SPECTRUM: return "Spectrum";
        case WINDOW_V_PEAK: return "Peak Meter";
    }
    
    return NULL;
}

const char *get_window_internal_name(int window) {
    ASSERT(window >= 0 && window <= WINDOW__COUNT);
    
    // These names are used in the layout INI so don't change them
    switch (window) {
        case WINDOW_LIBRARY: return "Library";
        case WINDOW_QUEUE: return "Queue";
        case WINDOW_ALBUM_LIST: return "AlbumList";
        case WINDOW_SEARCH_RESULTS: return "SearchResults";
        case WINDOW_METADATA: return "Metadata";
        case WINDOW_USER_PLAYLISTS: return "UserPlaylists";
        case WINDOW_PLAYLIST_TRACKS: return "PlaylistTracks";
        case WINDOW_THEME_EDITOR: return "ThemeEditor";
        case WINDOW_METADATA_EDITOR: return "MetadataEditor";
        case WINDOW_V_SPECTRUM: return "Spectrum";
        case WINDOW_V_PEAK: return "ChannelPeaks";
    }
    
    return NULL;
}

int get_window_from_name(const char *name) {
    
    for (int i = 0; i < WINDOW__COUNT; ++i) {
        if (!strcmp(name, get_window_internal_name(i))) {
            return i;
        }
    }
    
    return -1;
}

int is_window_open(int window) {
    return ui.show_window[window];
}

void set_window_showing(int window, bool showing) {
    ui.show_window[window] = showing;
}

void ui_push_mini_font() {
    if (ui.mini_font) ImGui::PushFont(ui.mini_font);
}

void ui_pop_mini_font() {
    if (ui.mini_font) ImGui::PopFont();
}

static void bring_window_to_front(int window) {
    ui.show_window[window] = true;
    ui.focus_window[window] = true;
}

static Playlist *get_selected_user_playlist(Path_Index *path = NULL) {
    for (u32 i = 0; i < ui.user_playlists.count; ++i) {
        if (ui.user_playlists[i].get_id() == ui.selected_user_playlist_id) {
            if (path) *path = ui.user_playlist_paths[i];
            return &ui.user_playlists[i];
        }
    }
    
    return NULL;
}

static void save_user_playlist(u32 index) {
    Playlist& playlist = ui.user_playlists[index];
    wchar_t path[PATH_LENGTH];
    retrieve_file_path(ui.user_playlist_paths[index], path, PATH_LENGTH);
    save_playlist_to_file(playlist, path);
}

static void play_track(const Track& track) {
    wchar_t track_path[PATH_LENGTH];
    ui.current_track = track;
    library_get_track_path(track, track_path);
    playback_load_file(track_path);
    
    Metadata md;
    library_get_track_metadata(track, &md);
    
    set_window_title_message("%s - %s", md.artist, md.title);
    notify(NOTIFY_NEW_TRACK_PLAYING);
}

static void play_playlist(const Playlist& playlist, Track *start_track = NULL) {
    ASSERT(playlist.get_id() != ui.queue_id);
    Track track;
    i32 start_index = 0;
    if (!playlist.tracks.count) return;
    
    ui.queue.clear();
    playlist.copy_to(ui.queue);
    
    if (ui.shuffle_on) ui.queue.shuffle();
    
    if (start_track) {
        start_index = ui.queue.index_of_track(*start_track);
        if (start_index == -1) {
            start_index = 0;
            log_debug("Requested track not in playlist!\n");
        }
    }
    
    track = ui.queue.tracks[start_index];
    ui.queue_position = start_index;
    ui.current_playlist_id = playlist.get_id();
    
    play_track(track);
}

static void go_to_queue_position(i32 position) {
    if (ui.queue.tracks.count == 0) return;
    position = ui.queue.repeat(position);
    Track track = ui.queue.tracks[position];
    play_track(track);
    ui.queue_position = position;
}

void ui_play_next_track() {
    i32 position = ui.queue_position;
    go_to_queue_position(position + 1);
}

void ui_play_previous_track() {
    i32 position = ui.queue_position;
    go_to_queue_position(position - 1);
}

Track ui_get_playing_track() {
    return ui.current_track;
}

static void load_state() {
    auto callback = [](void *data, const char *section, const char *key, const char *value) -> int {
        if (!strcmp(key, "iVolume")) {
            float volume = clamp((float)atof(value), 0.f, 1.f);
            playback_set_volume(volume);
        }
        else if (!strcmp(key, "bShuffle")) {
            ui.shuffle_on = atoi(value) != 0;
        }
        return 1;
    };
    
    ini_parse(STATE_PATH, callback, NULL);
}

static void save_state() {
    FILE *f = fopen(STATE_PATH, "w");
    if (!f) return;
    
    fprintf(f, "iVolume = %f\n", playback_get_volume());
    fprintf(f, "bShuffle = %d\n", ui.shuffle_on);
    
    fclose(f);
}

static void apply_filter_query(Playlist& playlist) {
    const Array<Track>& tracks = playlist.tracks;
    Array<Track>& output = ui.search_results.tracks;
    char filter[FILTER_STRING_MAX];
    string_to_lower(playlist.filter, filter, sizeof(filter));
    
    zero_array(playlist.filter, ARRAY_LENGTH(playlist.filter));
    
    output.clear();
    for (u32 i = 0; i < tracks.count; ++i) {
        const Track& track = tracks[i];
        if (track_meets_filter(track, filter)) {
            output.append(track);
        }
    }
    
    bring_window_to_front(WINDOW_SEARCH_RESULTS);
}

static void show_library() {
    Track_List_Action action = {};
    
    if (begin_window_drag_drop_target("##library_drag_drop")) {
        ui.library_altered |= accept_drag_drop_to_playlist(ui.library);
        ImGui::EndDragDropTarget();
    }
    
    show_playlist_track_list("##library", ui.library, ui.current_track, &action);
    if (action.user_requested_track) {
        Track& track = ui.library.tracks[action.requested_track_index];
        play_playlist(ui.library, &track);
    }
    
    ui.library_altered |= action.user_altered_playlist;
    
    if (action.want_apply_filter) {
        apply_filter_query(ui.library);
    }
}

static void show_queue() {
    Track_List_Action action = {};
    // The queue is unsortable because it messes up the queue position
    show_playlist_track_list("##queue", ui.queue, ui.current_track, &action,
                             TRACK_LIST_FLAGS_NO_SORT|TRACK_LIST_FLAGS_NO_FILTER);
    if (action.user_requested_track) {
        go_to_queue_position(action.requested_track_index);
    }
    
    if (begin_window_drag_drop_target("##queue_drag_drop")) {
        accept_drag_drop_to_playlist(ui.queue);
        ImGui::EndDragDropTarget();
    }
}

static void show_search_results() {
    Track_List_Action action = {};
    show_playlist_track_list("##search_results", ui.search_results, ui.current_track, &action,
                             TRACK_LIST_FLAGS_NO_FILTER);
    if (action.user_requested_track) {
        Track& track = ui.search_results.tracks[action.requested_track_index];
        play_playlist(ui.search_results, &track);
    }
}

// Call upon playing a new track
static void update_detailed_metadata() {
    Track track = ui.current_track;
    if (!track) return;
    
    if (ui.detailed_metadata_track != track) {
        wchar_t path[PATH_LENGTH];
        Image cover_art;
        library_get_track_path(track, path);
        read_detailed_file_metadata(path, &ui.detailed_metadata, &cover_art);
        ui.detailed_metadata_track = track;
        
        if (ui.cover_art)
            destroy_texture(&ui.cover_art);
        
        if (cover_art.data) {
            ui.cover_art = create_texture_from_image(&cover_art);
            free_image(&cover_art);
        }
    }
    
}

static void show_detailed_metadata() {
    if (!ui.current_track) {
        ImGui::TextDisabled("No metadata currently loaded");
        return;
    }
    show_detailed_metadata_table("##metadata", ui.detailed_metadata, ui.cover_art);
}

static void show_metadata_editor() {
    static Detailed_Metadata md;
    static Track md_track;

    if (!ui.metadata_editor_track) {
        ImGui::TextDisabled("No track selected");
        return;
    }

    if (ui.metadata_editor_track != md_track) {
        wchar_t path[PATH_LENGTH];
        md_track = ui.metadata_editor_track;
        library_get_track_path(md_track, path);
        read_detailed_file_metadata(path, &md);
    }

    ImGuiTableFlags table_flags = ImGuiTableFlags_RowBg|ImGuiTableFlags_SizingStretchProp;

    struct {
        const char *id;
        const char *name;
        char *buf;
        int buf_size;
    } md_strings[] = {
        {"##title", "Title", md.title, sizeof(md.title)},
        {"##artist", "Artist", md.artist, sizeof(md.artist)},
        {"##album", "Album", md.album, sizeof(md.album)},
        {"##Genre", "Genre", md.genre, sizeof(md.genre)},
    };

    ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
    if (ImGui::BeginTable("metadata_edit_table", 2, table_flags)) {
        ImGui::TableSetupColumn("key", 0.3f);
        ImGui::TableSetupColumn("value", 0.7f);

        for (auto str : md_strings) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(str.name);
            ImGui::TableSetColumnIndex(1);
            ImGui::InputText(str.id, str.buf, str.buf_size);
        }

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Year");
        ImGui::TableSetColumnIndex(1);
        ImGui::InputScalar("##year", ImGuiDataType_U32, &md.year);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Track number");
        ImGui::TableSetColumnIndex(1);
        ImGui::InputScalar("##trackno", ImGuiDataType_U32, &md.track_number);

        ImGui::EndTable();
    }
    ImGui::PopStyleColor();

    ImGui::TextUnformatted("Comment");
    ImGui::InputTextMultiline("##comment", md.comment, sizeof(md.comment));

    if (ImGui::Button("Save")) {
        wchar_t path[PATH_LENGTH];
        Metadata_Index md_index = library_get_track_metadata_index(md_track);
        library_get_track_path(md_track, path);
        if (show_confirm_dialog("Confirm metadata update", "Overwrite metadata for file %ls?", path)) {
            update_file_metadata(md_index, path, &md);
        }
    }
}

static void load_theme_from_file(const wchar_t *path) {
    auto callback =
    [](void *dont_care, const char *section, const char *key, const char *value) -> int {
        ImGuiStyle& style = ImGui::GetStyle();
        for (u32 i = 0; i < ImGuiCol_COUNT; ++i) {
            if (!strcmp(key, ImGui::GetStyleColorName(i))) {
                u32 color = (u32)strtoll(value, NULL, 16);
                style.Colors[i] = ImColor(color).Value;
            }
        }
        return 1;
    };
    
    FILE *file = _wfopen(path, L"r");
    if (!file) return;
    ini_parse_file(file, callback, NULL);
    fclose(file);
}

static void show_theme_editor() {
    bool dirty = show_theme_editor_gui();
    if (dirty) {
        ui.window_flags[WINDOW_THEME_EDITOR] |= ImGuiWindowFlags_UnsavedDocument;
    }
    else {
        ui.window_flags[WINDOW_THEME_EDITOR] &= ~ImGuiWindowFlags_UnsavedDocument;
    }
}

static bool is_playlist_name_in_use(const char *name) {
    for (u32 i = 0; i < ui.user_playlists.count; ++i) {
        if (!strcmp(ui.user_playlists[i].name, name)) return true;
    }
    
    return false;
}

static bool is_playlist_name_reserved(const char *name) {
    return !strcmp(name, ui.library.name) || !strcmp(name, ui.queue.name)
        || !strcmp(name, ui.search_results.name);
}

// Returns NULL if the name is valid, otherwise a description of why it isn't
static const char *validate_playlist_name(const char *name) {
    if (!name[0]) {
        return "Name cannot be empty";
    }
    else if (is_playlist_name_in_use(name)) {
        return "Name already in use";
    }
    else if (is_playlist_name_reserved(name)) {
        return "Name is reserved, please try a different name";
    }
    
    return NULL;
}

static void show_user_playlists() {
    static char new_playlist_name[PLAYLIST_NAME_MAX];
    static char rename_playlist_name[PLAYLIST_NAME_MAX];
    static int rename_playlist_index;
    if (ImGui::Button("+ New playlist") || ui.want_to_create_playlist_from_selection) {
        ImGui::OpenPopup("New playlist");
        memset(new_playlist_name, 0, sizeof(new_playlist_name));
    }
    
    //-
    // Show popup to name a new playlist
    ImGui::SetNextWindowSize(ImVec2(400, 0));
    if (ImGui::BeginPopupModal("New playlist")) {
        static const char *status_line;
        bool commit = false;
        ImGui::TextUnformatted("Name your playlist:");
        
        commit |= ImGui::InputText("##playlist_name", new_playlist_name, PLAYLIST_NAME_MAX,
                                   ImGuiInputTextFlags_EnterReturnsTrue);
        
        commit |= ImGui::Button("Create");
        
        if (commit) {
            status_line = validate_playlist_name(new_playlist_name);
            if (!status_line) {
                wchar_t save_path[512];
                Playlist new_playlist = {};
                new_playlist.set_name(new_playlist_name);
                
                if (ui.want_to_create_playlist_from_selection) {
                    ui.track_selection.copy_unique_to(new_playlist.tracks);
                    ui.want_to_create_playlist_from_selection = false;
                    new_playlist.sort();
                }
                
                generate_temporary_file_name(PLAYLIST_DIRECTORY_W, save_path, sizeof(save_path));
                save_playlist_to_file(new_playlist, save_path);
                
                ui.user_playlists.append(new_playlist);
                ui.user_playlist_paths.append(store_file_path(save_path));
                
                status_line = NULL;
                ImGui::CloseCurrentPopup();
            }
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            status_line = NULL;
            ui.want_to_create_playlist_from_selection = false;
            ImGui::CloseCurrentPopup();
        }
        
        if (status_line) ImGui::TextUnformatted(status_line);
        else ImGui::NewLine();
        
        ImGui::EndPopup();
    }
    //-
    
    //-
    // Show playlist table and handle interactions with it
    Playlist_List_Action action = {};
    show_playlist_list("##user_playlists", ui.user_playlists, ui.current_playlist_id, &action,
                       0, ui.selected_user_playlist_id);
    if (action.user_requested_playlist) {
        const Playlist& playlist = ui.user_playlists[action.requested_playlist_index];
        ui.selected_user_playlist_id = playlist.get_id();
        bring_window_to_front(WINDOW_PLAYLIST_TRACKS);
        play_playlist(playlist);
    }
    
    if (action.user_selected_playlist) {
        const Playlist& playlist = ui.user_playlists[action.selected_playlist_index];
        ui.selected_user_playlist_id = playlist.get_id();
        bring_window_to_front(WINDOW_PLAYLIST_TRACKS);
    }
    
    if (action.user_altered_playlist) {
        wchar_t save_path[PATH_LENGTH];
        const Playlist &playlist = ui.user_playlists[action.altered_playlist_index];
        retrieve_file_path(ui.user_playlist_paths[action.altered_playlist_index], 
                           save_path, PATH_LENGTH);
        save_playlist_to_file(playlist, save_path);
    }
    
    if (action.user_requested_delete_playlist) {
        u32 index = action.requested_delete_playlist_index;
        Playlist &playlist = ui.user_playlists[index];
        if (show_yes_no_dialog("Confirm Delete Playlist", "Delete playlist '%s'?", playlist.name)) {
            wchar_t save_path[PATH_LENGTH];
            retrieve_file_path(ui.user_playlist_paths[index], 
                               save_path, PATH_LENGTH);
            delete_file(save_path);
            playlist.tracks.free();
            ui.user_playlists.ordered_remove(index);
            ui.user_playlist_paths.ordered_remove(index);
        }
    }
    
    if (action.user_requested_rename_playlist) {
        rename_playlist_index = action.requested_rename_playlist_index;
        // Copy the playlist name to the rename playlist buffer
        strcpy(rename_playlist_name, 
               ui.user_playlists[action.requested_rename_playlist_index].name);
        ImGui::OpenPopup("Rename playlist");
    }
    //-
    
    //-
    // Show popup to rename an existing playlist
    ImGui::SetNextWindowSize(ImVec2(400, 0));
    if (ImGui::BeginPopupModal("Rename playlist")) {
        static const char *status_line;
        Playlist *playlist = &ui.user_playlists[rename_playlist_index];
        bool commit = false;
        ImGui::TextUnformatted("Rename playlist:");
        
        commit |= ImGui::InputText("##playlist_name", rename_playlist_name, PLAYLIST_NAME_MAX,
                                   ImGuiInputTextFlags_EnterReturnsTrue);
        
        commit |= ImGui::Button("Rename");
        
        if (commit) {
            status_line = validate_playlist_name(rename_playlist_name);
            if (!status_line){
                bool is_playing = playlist->get_id() == ui.current_playlist_id;
                
                playlist->set_name(rename_playlist_name);
                
                ui.current_playlist_id = playlist->get_id();
                save_user_playlist(rename_playlist_index);
            }
            status_line = NULL;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            status_line = NULL;
            ImGui::CloseCurrentPopup();
        }
        
        if (status_line) ImGui::TextUnformatted(status_line);
        else ImGui::NewLine();
        
        ImGui::EndPopup();
    }
    //-
}

static void show_album_list_view() {
    Playlist_List_Action action = {};
    if (ui.viewing_album_id) {
        i32 index = ui.album_ids.lookup(ui.viewing_album_id);
        if (index < 0) {
            ui.viewing_album_id = 0;
        }
        else {
            Track_List_Action action = {};
            Playlist& playlist = ui.albums[index];
            
            if (ImGui::Button("Back to albums")) ui.viewing_album_id = 0;
            ImGui::SameLine();
            if (ImGui::Button("Play")) play_playlist(playlist);
            
            show_playlist_track_list("##album_content", playlist,
                                     ui.current_track, &action, TRACK_LIST_FLAGS_NO_EDIT);
            
            if (action.user_requested_track) {
                Track track = playlist.tracks[action.requested_track_index];
                play_playlist(playlist, &track);
            }
            
            return;
        }
    }
    
    show_playlist_list("##album_list", ui.albums, ui.current_playlist_id, 
                       &action, PLAYLIST_LIST_FLAGS_SHOW_CREATOR|PLAYLIST_LIST_FLAGS_NO_EDIT);
    
    if (action.user_requested_playlist) {
        const Playlist& playlist = ui.albums[action.requested_playlist_index];
        ui.selected_user_playlist_id = playlist.get_id();
        play_playlist(playlist);
    }
    
    if (action.user_selected_playlist) {
        ui.viewing_album_id = ui.album_ids[action.selected_playlist_index];
    }
}

static void show_selected_playlist() {
    Path_Index save_path;
    Playlist *playlist = get_selected_user_playlist(&save_path);
    if (!playlist) {
        ImGui::TextDisabled("Select a playlist from \"Your Playlists\"");
        if (ImGui::Button("Open playlists")) {
            bring_window_to_front(WINDOW_USER_PLAYLISTS);
        }
        
        return;
    }
    
    bool altered = false;
    
    if (begin_window_drag_drop_target("##playlist_drag_drop")) {
        altered |= accept_drag_drop_to_playlist(*playlist);
        ImGui::EndDragDropTarget();
    }
    
    Track_List_Action action = {};
    show_playlist_track_list(playlist->name, *playlist, ui.current_track, &action);
    
    altered |= action.user_altered_playlist;
    
    if (action.user_requested_track) {
        play_playlist(*playlist, &playlist->tracks[action.requested_track_index]);
    }
    
    if (altered) {
        wchar_t path[PATH_LENGTH];
        retrieve_file_path(save_path, path, PATH_LENGTH);
        save_playlist_to_file(*playlist, path);
    }
    
    if (action.want_apply_filter) {
        apply_filter_query(*playlist);
    }
}

void show_ui() {
    ASSERT(ui.ready);
    ImGuiIO& io = ImGui::GetIO();
    const ImGuiStyle& style = ImGui::GetStyle();
    const char *filter_popup_name = "Search playlist";
    ui.filter_popup_id = ImGui::GetID(filter_popup_name);

    update_playback_analyzers(16.66f);
    
    // If the current track is not the track
    // we have detailed metadata for, load in the
    // new metadata. This must be done at the start of 
    // the frame because it frees the old texture for the cover art,
    // which UI functions might want to use.
    update_detailed_metadata();
    
    float menu_bar_height = 0.f;
    Preferences &prefs = get_preferences();
    
    if (ui.track_scan_thread) {
        u32 total_tracks = ui.track_scan_progress.total_track_count;
        u32 loaded_tracks = ui.track_scan_progress.tracks_loaded;
        u32 errors = ui.track_scan_progress.errors;
        bool done = ui.track_scan_progress.done;

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration;
        ImVec2 window_size = io.DisplaySize;
        ImVec2 window_pos;

        window_size.x *= 0.5f;
        window_size.y *= 0.5f;
        window_pos.x = (io.DisplaySize.x * 0.5f) - (window_size.x * 0.5f);
        window_pos.y = (io.DisplaySize.y * 0.5f) - (window_size.y * 0.5f);

        ImGui::SetNextWindowSize(window_size);
        ImGui::SetNextWindowPos(window_pos);
        if (ImGui::Begin("Adding Tracks", NULL, window_flags)) {
            ImGui::TextUnformatted(
                "Scanning files... This may take some time for a"
                "large number of files or files on a hard drive");
            ImGui::ProgressBar((f32)loaded_tracks / (f32)total_tracks, ImVec2(0, 0), "");
            ImGui::Text("%u / %u (%u errors)", loaded_tracks, total_tracks, errors);
        }
        ImGui::End();

        if (done) {
            thread_destroy(ui.track_scan_thread);
            ui.track_scan_thread = NULL;
            ui.track_scan_buffer.paths.free();
            ui.track_scan_buffer.path_pool.free();
            ui.track_scan_progress.done = false;
        }

        return;
    }

    //-
    // Main menu
    if (ImGui::BeginMainMenuBar()) {
        menu_bar_height = ImGui::GetWindowSize().y;
        
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Edit hotkeys")) {
                ui.show_hotkeys = true;
            }
            if (ImGui::MenuItem("Preferences")) {
                ui.show_prefs = true;
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Remove all invalid tracks")) {
                const char *confirmation_notice =
                    "Remove all missing/invalid tracks? This cannot be undone."
                    "This may take a while for a large library."
                    ;

                if (show_confirm_dialog("Confirm remove invalid tracks", confirmation_notice)) {
                    u32 count = playlist_remove_missing_tracks(ui.library);
                    // Skip search results and queue. With the queue it doesn't matter because invalid
                    // tracks will just be skipped anyway.

                    for (u32 i = 0; i < ui.user_playlists.count; ++i) {
                        playlist_remove_missing_tracks(ui.user_playlists[i]);
                    }

                    show_message_box(MESSAGE_BOX_TYPE_INFO, "Removed %u tracks", count);
                }
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Minimize to tray")) {
                notify(NOTIFY_MINIMIZE_TO_TRAY);
            }
            if (ImGui::MenuItem("Exit")) {
                notify(NOTIFY_QUIT);
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Library")) {
            show_add_files_menu(&ui.library);
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Playlist")) {
            Path_Index playlist_path;
            Playlist *playlist = get_selected_user_playlist(&playlist_path);
            
            if (playlist) {
                ImGui::SeparatorText(playlist->name);
                if (show_add_files_menu(playlist)) {
                    wchar_t save_path[PATH_LENGTH];
                    retrieve_file_path(playlist_path, save_path, PATH_LENGTH);
                    save_playlist_to_file(*playlist, save_path);
                }
            }
            else {
                ImGui::TextDisabled("No playlist selected");
            }
            
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("View")) {
            if (ImGui::BeginMenu("Windows")) {
                for (int i = 0; i < WINDOW__FIRST_VISUALIZER; ++i) {
                    const char *name = get_window_name(i);
                    if (ImGui::MenuItem(name, NULL, ui.show_window[i])) {
                        bring_window_to_front(i);
                    }
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Visualizers")) {
                for (int i = WINDOW__FIRST_VISUALIZER; i < WINDOW__COUNT; ++i) {
                    const char *name = get_window_name(i);
                    if (ImGui::MenuItem(name, NULL, ui.show_window[i])) {
                        bring_window_to_front(i);
                    }
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About")) {
                ui.show_about_window = true;
            }
            ImGui::EndMenu();
        }
        
        // Volume controls
        ImGui::Separator();
        ImGui::PushItemWidth(100.f);
        {
            float volume = playback_get_volume();
            int scaled_volume = (int)(volume * 100.f);
            if (ImGui::SliderInt("Volume", &scaled_volume, 0, 100, "%d%%")) {
                volume = (float)scaled_volume / 100.f;
                playback_set_volume(volume);
            }
        }
        ImGui::PopItemWidth();
        
        // Menu bar visualizer
        ImGui::Separator();
        switch (prefs.menu_bar_visualizer) {
        case MENU_BAR_VISUAL_PEAK_METER:
            peak_meter_widget("##peak_meter", ImVec2(150, 0));
            break;
        case MENU_BAR_VISUAL_SPECTRUM:
            show_spectrum_widget("##spectrum", 150);
            break;
        }
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Spectrum")) {
                prefs.menu_bar_visualizer = MENU_BAR_VISUAL_SPECTRUM;
                set_preferences_dirty();
            }
            if (ImGui::MenuItem("Peak Meter")) {
                prefs.menu_bar_visualizer = MENU_BAR_VISUAL_PEAK_METER;
                set_preferences_dirty();
            }
            ImGui::EndPopup();
        }
        
        // Playback controls
        ImGui::Separator();
        {
            bool paused = playback_get_state() != PLAYBACK_STATE_PLAYING;
            
            if (ImGui::MenuItem(SHUFFLE_ICON, NULL, ui.shuffle_on))
                ui.shuffle_on = !ui.shuffle_on;
            if (ImGui::MenuItem(PREV_TRACK_ICON))
                ui_play_previous_track();
            if (ImGui::MenuItem(paused ? PLAY_ICON : PAUSE_ICON))
                playback_toggle();
            if (ImGui::MenuItem(NEXT_TRACK_ICON))
                ui_play_next_track();
        }
        
        ImGui::Separator();
        // Playback timer
        {
            char current[64];
            char duration[64];
            format_time(playback_get_position_millis()/1000, current, 64);
            format_time(playback_get_duration_millis()/1000, duration, 64);
            
            ImGui::Text("%s/%s", current, duration);
            
            // Seek slider
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 5));
            static bool active_last_frame;
            static float position;
            bool active_now;
            active_now = circle_handle_slider("##seek_slider", &position, 0.f, 1.f,
                                              ImVec2(0, menu_bar_height - (style.FramePadding.y*2.f)));
            ImGui::PopStyleVar();
            
            if (active_last_frame && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                log_debug("%g\n", position);
                playback_seek_to_millis((i64)(position * (float)playback_get_duration_millis()));
            }
            if (!active_now) position = (float)playback_get_position_millis()/(float)playback_get_duration_millis();
            
            active_last_frame = active_now;
        }
        
        
        ImGui::EndMainMenuBar();
        
    }
    //-

    //-
    // Status bar
    if (begin_status_bar()) {
        if (ui.current_track) {
            Metadata md;
            library_get_track_metadata(ui.current_track, &md);
            ImGui::Text("Now playing: %s - %s", md.artist, md.title);
        }
        end_status_bar();
    }
    ImGui::End();
    //-
    

    //-
    // Set up main dock space
    {
        ImGuiWindowFlags window_flags =
            ImGuiWindowFlags_NoBringToFrontOnFocus|
            ImGuiWindowFlags_NoBackground|
            ImGuiWindowFlags_NoNavFocus|
            ImGuiWindowFlags_NoDecoration;
        ImGuiDockNodeFlags dock_flags = ImGuiDockNodeFlags_PassthruCentralNode;
        ImVec2 padding = ImGui::GetStyle().WindowPadding;
        ImVec2 display_size = ImGui::GetIO().DisplaySize;
        ImVec2 window_size = {
            display_size.x + (padding.x*2.f),
            // - 2*menu_bar_height for status bar + menu bar
            display_size.y - (menu_bar_height*2.f) + (padding.y*2.f),
        };
        
        ImGui::SetNextWindowPos(ImVec2(-padding.x, menu_bar_height - padding.y));
        ImGui::SetNextWindowSize(window_size);
        bool showing = ImGui::Begin("##main_dockspace_window", NULL, window_flags);
        
        if (!showing) dock_flags |= ImGuiDockNodeFlags_KeepAliveOnly;
        
        ImGui::DockSpace(ImGui::GetID("##main_dockspace"), ImVec2(0, 0), dock_flags);
        
        ImGui::End();
    }
    //-
    
    //-
    // Show visible windows
    for (u32 i = 0; i < WINDOW__COUNT; ++i) {
        if (!ui.show_window[i]) continue;
        char title[128];
        // Use the internal name as the ID so we can change the window name without breaking
        // layout settings
        snprintf(title, 128, "%s###%s", get_window_name(i), get_window_internal_name(i));
        
        if (ui.focus_window[i]) {
            ImGui::SetNextWindowFocus();
            ui.focus_window[i] = false;
        }
        
        if (ImGui::Begin(title, &ui.show_window[i], ui.window_flags[i])) {
            // This is needed for handling drag-drop from shell
            ui.is_window_hovered[i] = ImGui::IsWindowHovered();
            
            if (ui.window_show_fn[i]) ui.window_show_fn[i]();
        }
        ImGui::End();
    }
    //-
    
    //-
    // Preference editor
    if (ui.show_prefs) {
        if (ImGui::Begin("Preferences", &ui.show_prefs)) {
            show_prefs_editor();
        }
        ImGui::End();
    }
    //-
    
    //-
    // Hotkey editor
    if (ui.show_hotkeys) {
        if (ImGui::Begin("Edit hotkeys", &ui.show_hotkeys)) {
            show_hotkey_editor();
        }
        ImGui::End();
    }
    //-
    
    //-
    // About
    if (ui.show_about_window) {
        if (ImGui::Begin("About", &ui.show_about_window)) {
            show_about();
        }
        ImGui::End();
    }
    //-
}

void init_ui() {
    START_TIMER(init_ui, "Initialize UI");
    for (const wchar_t *d : REQUIRED_DIRECTORIES) {
        if (!does_file_exist(d)) {
            create_directory(d);
        }
    }
    
    register_imgui_settings_handler();
    
    // Load playlists from Playlists folder
    {
        auto load_playlist_iterator = 
        [](void *dont_care, const wchar_t *path, bool is_folder) -> Recurse_Command {
            Playlist playlist = {};
            wlog_debug(L"Load playlist: %s\n", path);
            if (load_playlist_from_file(path, playlist)) {
                ui.user_playlists.append(playlist);
                ui.user_playlist_paths.append(store_file_path(path));
            }
            
            return RECURSE_CONTINUE;
        };
        
        START_TIMER(load_playlists, "Load playlists");
        for_each_file_in_folder(PLAYLIST_DIRECTORY_W, load_playlist_iterator, NULL);
        STOP_TIMER(load_playlists);
    }
    
    START_TIMER(load_library, "Load library");
    load_playlist_from_file(LIBRARY_PATH, ui.library);
    STOP_TIMER(load_library);
    
    if (ui.library.tracks.count) for (const Track& track : ui.library.tracks) {
        add_to_albums(track);
    }
    
    ui.library.set_name("Library");
    ui.queue.set_name("Queue");
    ui.search_results.set_name("Search Results");
    
    // Get common playlist IDs so we don't need to constantly rehash them
    ui.library_id = ui.library.get_id();
    ui.queue_id = ui.queue.get_id();
    ui.search_results_id = ui.search_results.get_id();
    
    // Set up hooks for showing window contents
    ui.window_show_fn[WINDOW_LIBRARY] = &show_library;
    ui.window_show_fn[WINDOW_QUEUE] = &show_queue;
    ui.window_show_fn[WINDOW_METADATA] = &show_detailed_metadata;
    ui.window_show_fn[WINDOW_USER_PLAYLISTS] = &show_user_playlists;
    ui.window_show_fn[WINDOW_PLAYLIST_TRACKS] = &show_selected_playlist;
    ui.window_show_fn[WINDOW_SEARCH_RESULTS] = &show_search_results;
    ui.window_show_fn[WINDOW_THEME_EDITOR] = &show_theme_editor;
    ui.window_show_fn[WINDOW_ALBUM_LIST] = &show_album_list_view;
    ui.window_show_fn[WINDOW_METADATA_EDITOR] = &show_metadata_editor;
    ui.window_show_fn[WINDOW_V_SPECTRUM] = &show_spectrum_ui;
    ui.window_show_fn[WINDOW_V_PEAK] = &show_channel_peaks_ui;
    
    ui.window_flags[WINDOW_METADATA] = ImGuiWindowFlags_AlwaysVerticalScrollbar;
    
    if (!does_file_exist(L"imgui.ini")) {
        ImGui::LoadIniSettingsFromMemory(DEFAULT_LAYOUT_INI);
    }
    
    load_state();
    atexit(&save_all_state);
    
    set_default_theme();
    
    // Load mini font
    {
        f32 scaled_size = get_dpi_scale() * 9.f;
        ImFontConfig cfg = ImFontConfig();
        
        ui.mini_font = ui.mini_font_atlas.AddFontFromFileTTF("C:\\Windows\\Fonts\\seguisb.ttf", scaled_size, &cfg);
        ui.mini_font_atlas.Build();
        
        u8 *tex_data;
        int tex_width, tex_height;
        
        ui.mini_font_atlas.GetTexDataAsRGBA32(&tex_data, &tex_width, &tex_height);
        
        Image img = {};
        img.data = tex_data;
        img.format = IMAGE_FORMAT_R8G8B8A8;
        img.width = tex_width;
        img.height = tex_height;
        
        Texture *texture = create_texture_from_image(&img);
        
        log_debug("Created mini font atlas: %dx%d\n", tex_width, tex_height);
        
        ui.mini_font_atlas.SetTexID((ImTextureID)texture);
        ui.mini_font_atlas.ClearTexData();
    }
    
    ui.ready = true;
    STOP_TIMER(init_ui);
}

static void save_all_state() {
    save_state();
    save_playlist_to_file(ui.library, LIBRARY_PATH);
}

// Show menu items to add files or folders to a playlist
bool show_add_files_menu(Playlist *playlist) {
    if (ImGui::MenuItem("Add files")) {
        bool commit = open_file_multiselect_dialog(FILE_TYPE_AUDIO, &add_tracks_to_async_scan,
            NULL);

        if (commit) {
            begin_add_tracks_async_scan(playlist);
            return true;
        }
    }

    if (ImGui::MenuItem("Add folders")) {
        bool commit = open_folder_multiselect_dialog(FILE_TYPE_AUDIO, &add_tracks_to_async_scan, 
            NULL);

        if (commit) {
            begin_add_tracks_async_scan(playlist);
            return true;
        }
    }

    return false;
}

void show_track_context_menu(Playlist& from_playlist, u32 track_index) {
    u32 from_playlist_id = from_playlist.get_id();
    if (ImGui::BeginMenu("Add to playlist")) {
        if (ui.user_playlists.count) for (u32 i = 0; i < ui.user_playlists.count; ++i) {
            Playlist& playlist = ui.user_playlists[i];
            bool is_self = playlist.get_id() == from_playlist_id;
            
            if (is_self)
                ImGui::TextDisabled(playlist.name);
            else if (ImGui::MenuItem(playlist.name)) {
                ui.track_selection.copy_unique_to(playlist.tracks);
                playlist.sort();
                save_user_playlist(i);
            }
        }
        
        if (ui.user_playlists.count) ImGui::Separator();
        
        if (ImGui::MenuItem("New playlist...")) {
            ui.want_to_create_playlist_from_selection = true;
        }

        ImGui::EndMenu();
    }
    
    if (from_playlist_id != ui.queue_id) {
        ImGui::Separator();
        if (ImGui::MenuItem("Add to queue")) {
            if (ui.shuffle_on) shuffle_tracks(ui.track_selection);
            ui.track_selection.copy_unique_to(ui.queue.tracks);
        }
        if (ImGui::MenuItem("Play")) {
            ui.queue.clear();
            if (ui.shuffle_on) shuffle_tracks(ui.track_selection);
            ui.track_selection.copy_unique_to(ui.queue.tracks);
            go_to_queue_position(0);
        }
    }
    
    ImGui::Separator();

    if (ImGui::MenuItem("Edit metadata")) {
        ui.metadata_editor_track = from_playlist.tracks[track_index];
        bring_window_to_front(WINDOW_METADATA_EDITOR);
    }
}

static bool edit_path(const char *label, char *path, File_Type file_type) {
    bool commit = false;
    commit |= ImGui::InputText(label, path, PATH_LENGTH, ImGuiInputTextFlags_EnterReturnsTrue);
    
    ImGui::SameLine();
    if (ImGui::Button("Browse")) {
        wchar_t wpath[PATH_LENGTH];
        if (open_file_select_dialog(file_type, wpath, PATH_LENGTH)) {
            wchar_to_utf8(wpath, path, PATH_LENGTH);
            commit = true;
        }
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Remove")) {
        zero_array(path, PATH_LENGTH);
        commit = true;
    }
    
    return commit;
}

static void show_prefs_editor() {
    Preferences& prefs = get_preferences();
    bool apply = false;
    
    if (ImGui::BeginTable("##prefs_table", 2)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.2f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.8f);
        
        ImGui::PushID("background");
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Background");
        ImGui::TableSetColumnIndex(1);
        apply |= edit_path("##background", prefs.background, FILE_TYPE_IMAGE);
        ImGui::PopID();
        
        ImGui::PushID("theme");
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Theme");
        ImGui::TableSetColumnIndex(1);
        //apply |= edit_path("##theme", prefs.theme, FILE_TYPE_INI);
        if (ImGui::BeginCombo("##select_theme", prefs.theme)) {
            const char *new_theme = show_theme_selector_gui();
            if (new_theme) {
                strncpy0(prefs.theme, new_theme, sizeof(prefs.theme));
                apply = true;
            }
            
            ImGui::EndCombo();
        }
        ImGui::PopID();
        
        ImGui::PushID("font");
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Font");
        ImGui::TableSetColumnIndex(1);
        apply |= edit_path("##font", prefs.font, FILE_TYPE_FONT);
        ImGui::PopID();
        
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Font Size");
        ImGui::TableSetColumnIndex(1);
        apply |= ImGui::SliderInt("##font_size", &prefs.font_size,
                                  Preferences::FONT_SIZE_MIN,
                                  Preferences::FONT_SIZE_MAX);
        
        ImGui::PushID("iconfont");
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Icon Font Size");
        ImGui::TableSetColumnIndex(1);
        apply |= ImGui::SliderInt("##icon_font_size", &prefs.icon_font_size,
                                  Preferences::FONT_SIZE_MIN,
                                  Preferences::FONT_SIZE_MAX);
        ImGui::PopID();
        
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Close Policy");
        ImGui::TableSetColumnIndex(1);
        if (ImGui::BeginCombo("##close_policy", close_policy_to_string(prefs.close_policy))) {
            for (int i = 0; i < CLOSE_POLICY__COUNT; ++i) {
                if (ImGui::Selectable(close_policy_to_string(i), prefs.close_policy == i)) {
                    prefs.close_policy = i;
                    apply = true;
                }
            }
            ImGui::EndCombo();
        }
        
        ImGui::EndTable();
    }
    if (apply) apply_preferences();
}

static void show_hotkey_editor() {
    ImGuiTableFlags table_flags = ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInner;
    
    if (ImGui::BeginTable("##hotkeys", 2, table_flags)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.3f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.7f);
        char value[256];
        
        for (int i = 0; i < HOTKEY__COUNT; ++i) {
            const char *name = get_hotkey_name(i);
            bool being_captured = is_hotkey_being_captured(i);
            ImGui::PushID(name);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(name);
            ImGui::TableSetColumnIndex(1);
            if (being_captured) ImGui::SetKeyboardFocusHere();
            if (get_hotkey_bind_string(i, value, sizeof(value))) {
                if (ImGui::Selectable(value, being_captured)) {
                    capture_next_input_and_bind_to_hotkey(i);
                }
            }
            else if (ImGui::Selectable("##not_set", being_captured)) {
                capture_next_input_and_bind_to_hotkey(i);
            }
            ImGui::PopID();
        }
        
        ImGui::EndTable();
    }
}

static u32 get_highest_selection_index_before(const Playlist& playlist, const Track& track) {
    u32 ret = 0;
    
    for (u32 i = 0; i < playlist.tracks.count; ++i) {
        if (playlist.tracks[i] == track) break;
        if (ui.track_selection.contains(playlist.tracks[i])) ret = i;
    }
    
    return ret;
}

void select_track_in_playlist(Playlist& playlist, u32 track_index) {
    u32 playlist_id = playlist.get_id();
    if (playlist_id != ui.track_selection_playlist_id) {
        ui.track_selection_playlist_id = playlist_id;
        ui.track_selection.clear();
    }
    
    if (ImGui::IsKeyDown(ImGuiMod_Shift)) {
        if (!playlist.filter[0]) {
            u32 highest_index = get_highest_selection_index_before(playlist, playlist.tracks[track_index]);
            ui.track_selection.clear();
            playlist.tracks.copy_unique_range_to(highest_index, track_index, ui.track_selection);
        }
        else {
            // Selecting a track range while the tracks are being filtered
            // will not only be unintuitive for the user but an absoulte pain
            // to implement. So add the track to the selection as if the user
            // is holding ctrl instead
            ui.track_selection.append_unique(playlist.tracks[track_index]);
        }
    }
    else {
        if (!ImGui::IsKeyDown(ImGuiMod_Ctrl)) ui.track_selection.clear();
        ui.track_selection.append_unique(playlist.tracks[track_index]);
    }
}

void select_whole_playlist(Playlist& playlist) {
    u32 playlist_id = playlist.get_id();
    ui.track_selection_playlist_id = playlist_id;
    ui.track_selection.clear();
    if (!playlist.filter[0]) {
        playlist.tracks.copy_to(ui.track_selection);
    }
    else {
        char filter[FILTER_STRING_MAX];
        string_to_lower(playlist.filter, filter, sizeof(filter));
        for (u32 i = 0; i < playlist.tracks.count; ++i) {
            if (track_meets_filter(playlist.tracks[i], filter)) {
                ui.track_selection.append(playlist.tracks[i]);
            }
        }
    }
}

bool is_track_selected(const Track& track) {
    return ui.track_selection.contains(track);
}

void remove_selected_tracks_from_playlist(Playlist& playlist) {
    for (u32 i = 0; i < ui.track_selection.count; ++i) {
        const Track& track = ui.track_selection[i];
        i32 index = playlist.index_of_track(track);
        if (index >= 0) playlist.tracks.ordered_remove(index);
    }
}

void clear_track_selection() {
    ui.track_selection.clear();
}

void load_ui_theme(const char *path) {
    ImGui::StyleColorsDark();
    if (path && path[0]) {
        wchar_t wpath[PATH_LENGTH];
        utf8_to_wchar(path, wpath, PATH_LENGTH);
        load_theme_from_file(wpath);
    }
}

void begin_track_drag_drop() {
    const Array<Track>& payload = ui.track_selection;
    ImGui::SetDragDropPayload(DRAG_DROP_PAYLOAD_TYPE_TRACKS, payload.data,
                              sizeof(Track) * payload.count);
    ImGui::SetTooltip("%u tracks", payload.count);
}

bool accept_drag_drop_to_playlist(Playlist& playlist) {
    const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DRAG_DROP_PAYLOAD_TYPE_TRACKS);
    
    if (payload) {
        Array<Track> tracks = {};
        ASSERT(payload->DataSize % sizeof(Track) == 0);
        
        tracks.data = (Track*)payload->Data;
        tracks.count = payload->DataSize / sizeof(Track);
        
        playlist.add_tracks(tracks);
        playlist.sort();
        return true;
    }
    else if (ImGui::AcceptDragDropPayload("FILES")) {
        const File_Drag_Drop_Payload& payload = get_file_drag_drop_payload();
        
        for (u32 i = 0; i < payload.offsets.count; ++i) {
            const wchar_t *path = &payload.string_pool[payload.offsets[i]];
            add_tracks_to_async_scan(NULL, path, is_path_a_folder(path));
        }
        begin_add_tracks_async_scan(&playlist);
        return true;
    }
    
    return false;
}

static void show_about() {
    show_license_info();
}

static Recurse_Command async_file_scan_iterator(void *target_ptr, const wchar_t *path, bool is_folder) {
    Playlist *target = (Playlist*)target_ptr;


    if (is_folder) {
        for_each_file_in_folder(path, &async_file_scan_iterator, target);
    }
    else {
        Track track = library_add_track(path);
        if (!track) {
            ui.track_scan_progress.errors++;
            return RECURSE_CONTINUE;
        }

        target->add_track(track);
        add_to_albums(track);
        if (target != &ui.library) {
            ui.library.add_track(track);
        }
        ui.library_altered = true;
        ui.track_scan_progress.tracks_loaded++;
    }

    return RECURSE_CONTINUE;
}

static Recurse_Command file_counting_iterator(void *value_ptr, const wchar_t *path, bool is_folder) {
    u32 *value = (u32*)value_ptr;

    if (is_folder) {
        for_each_file_in_folder(path, &file_counting_iterator, value);
    }
    else if (is_supported_file(path)) {
        *value += 1;
    }

    return RECURSE_CONTINUE;
}

static int async_file_scan_thread_func(void *target_ptr) {
    Playlist *target = (Playlist*)target_ptr;
    u32 input_count = ui.track_scan_buffer.paths.count;
    u32 file_count = 0;
    const Array<wchar_t>& path_pool = ui.track_scan_buffer.path_pool;
    const Array<u32>& paths = ui.track_scan_buffer.paths;

    for (u32 i = 0; i < input_count; ++i) {
        const wchar_t *path = &path_pool[paths[i]];
        file_counting_iterator(&file_count, path, is_path_a_folder(path));
    }

    ui.track_scan_progress.total_track_count = file_count;

    for (u32 i = 0; i < input_count; ++i) {
        const wchar_t *path = &path_pool[paths[i]];
        async_file_scan_iterator(target, path, is_path_a_folder(path));
    }

    ui.track_scan_progress.done = true;

    return 0;
}

static void begin_add_tracks_async_scan(Playlist *target) {
    ASSERT(ui.track_scan_progress.done == false);
    ui.track_scan_progress.total_track_count = 0;
    ui.track_scan_progress.tracks_loaded = 0;
    ui.track_scan_progress.errors = 0;
    ui.track_scan_thread = thread_create(target, &async_file_scan_thread_func);
}
    