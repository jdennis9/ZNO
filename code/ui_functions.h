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
#ifndef UI_FUNCTIONS_H
#define UI_FUNCTIONS_H

#include "defines.h"
#include "array.h"
#include "playlist.h"
#include "platform.h"
#include "video.h"
#include <imgui.h>

#define DRAG_DROP_PAYLOAD_TYPE_TRACKS "TRACKS"

enum {
    WINDOW_QUEUE,
    WINDOW_ALBUM_LIST,
    WINDOW_METADATA,
    WINDOW_USER_PLAYLISTS,
    WINDOW_PLAYLIST_TRACKS,
    WINDOW_THEME_EDITOR,
    WINDOW_SEARCH_RESULTS,
    WINDOW_LIBRARY,
    WINDOW_METADATA_EDITOR,
    WINDOW__FIRST_VISUALIZER,
    WINDOW_V_SPECTRUM = WINDOW__FIRST_VISUALIZER,
    WINDOW__COUNT,
};

struct Add_Tracks_Iterator_State {
    Playlist *target;
    u32 track_count;
};

//-
// From ui.cpp
Recurse_Command add_tracks_to_playlist_iterator(void *in_data, const wchar_t *path, bool is_folder);
void show_track_context_menu(Playlist& playlist, u32 track_index);
void select_track_in_playlist(Playlist& playlist, u32 track_index);
bool is_track_selected(const Track& track);
void clear_track_selection();
void remove_selected_tracks_from_playlist(Playlist& playlist);
void select_whole_playlist(Playlist& playlist);
// Start a drag-drop with the current track selection as the payload
void begin_track_drag_drop();
// Call this within ImGui::Begin/EndDragDropTarget. Accepts files from windows shell as well
bool accept_drag_drop_to_playlist(Playlist& playlist);
// WINDOW_
const char *get_window_name(int window);
// Get internal name of window used for saving window state
const char *get_window_internal_name(int window);
int get_window_from_name(const char *name);
int is_window_open(int window);
void set_window_showing(int window, bool showing);
//-

//-
// From ui_functions.cpp
bool show_playlist_selectable(const Playlist& playlist, bool playing, ImGuiSelectableFlags flags);

typedef u32 Playlist_List_Flags;
enum {
    // Add an extra column on the left for playlist.creator
    // Also makes the table resizable
    PLAYLIST_LIST_FLAGS_SHOW_CREATOR = 1<<0,
    // Don't allow adding or removing tracks, deleting playlists or renaming playlists
    // Essentially disables the right-click context menu
    PLAYLIST_LIST_FLAGS_NO_EDIT = 1<<1,
};
struct Playlist_List_Action {
    u32 requested_playlist_index;
    u32 altered_playlist_index;
    u32 selected_playlist_index;
    u32 requested_delete_playlist_index;
    u32 requested_rename_playlist_index;
    bool user_altered_playlist; // User added tracks to a playlist
    bool user_requested_playlist; // User wants to play a playlist
    bool user_selected_playlist; // User selected a playlist (left-clicked)
    bool user_requested_delete_playlist;
    bool user_requested_rename_playlist;
};
void show_playlist_list(const char *str_id, Array<Playlist>& playlists, 
                        u32 playing_playlist_id, Playlist_List_Action *action,
                        Playlist_List_Flags flags = 0, u32 selected_playlist_id = 0);

typedef u32 Track_List_Flags;
enum {
    TRACK_LIST_FLAGS_NO_SORT = 1<<0,
    TRACK_LIST_FLAGS_NO_EDIT = 1<<1,
    TRACK_LIST_FLAGS_NO_FILTER = 1<<2,
};
struct Track_List_Action {
    u32 requested_track_index;
    bool user_requested_track;
    bool user_altered_playlist;
    bool want_apply_filter;
};
void show_playlist_track_list(const char *str_id, Playlist& playlist, Track current_track, 
                              Track_List_Action *action, Track_List_Flags flags = 0);

void show_detailed_metadata_table(const char *str_id, const Detailed_Metadata& metadata, Texture *cover_art);
// Show menu items to add files or folders to a playlist
bool show_add_files_menu(Playlist *playlist);
bool save_playlist_to_file(const Playlist& playlist, const wchar_t *path);
bool load_playlist_from_file(const wchar_t *path, Playlist& playlist);
//-

//-
// From ui_custom.cpp. These all use imgui_internal.h
bool circle_handle_slider(const char *str_id, float *value, float min, float max, ImVec2 size);
// @TODO: Separate into left/right channels
void peak_meter_widget(const char *str_id, float value, ImVec2 size);
// Calls ImGui::Begin() so make sure to call ImGui::End() no matter the return value is!
bool begin_status_bar();
void end_status_bar();
// Begins a drag drop target from the current window
bool begin_window_drag_drop_target(const char *str_id);
void register_imgui_settings_handler();
//-

//-
// From about.cpp
void show_license_info();
//-

#endif //UI_FUNCTIONS_H

