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
#include "ui_functions.h"
#include <imgui.h>

bool show_playlist_selectable(const Playlist& playlist, bool playing, ImGuiSelectableFlags flags) {
    return ImGui::Selectable(playlist.name, playing, flags);
}

bool is_item_double_clicked() {
    return ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
}

void show_playlist_list(const char *str_id, Array<Playlist>& playlists, 
                        u32 playing_playlist_id, Playlist_List_Action *action, Playlist_List_Flags flags,
                        u32 selected_playlist_id) {
    ImGuiTableFlags table_flags = ImGuiTableFlags_BordersInner;
    *action = Playlist_List_Action{};
    
    bool show_creator = (flags & PLAYLIST_LIST_FLAGS_SHOW_CREATOR) != 0;
    bool no_edit = (flags & PLAYLIST_LIST_FLAGS_NO_EDIT);
    
    if (show_creator) 
        table_flags |= ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_Resizable;
    
    if (ImGui::BeginTable(str_id, 2 + show_creator, table_flags)) {
        //-
        // Setup columns
        if (show_creator) {
            ImGui::TableSetupColumn("By", 0, 150.f);
            ImGui::TableSetupColumn("Title", 0, 150.f);
            ImGui::TableSetupColumn("No. Tracks", 0, 20.f);
        }
        else {
            ImGui::TableSetupColumn("Title");
            ImGui::TableSetupColumn("No. Tracks", ImGuiTableColumnFlags_WidthStretch, 0.2f);
        }
        
        if (show_creator) {
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableHeadersRow();
        }
        //-
        
        for (u32 i = 0; i < playlists.count; ++i) {
            Playlist& playlist = playlists[i];
            u32 playlist_id = playlist.get_id();
            bool is_playing = playlist_id == playing_playlist_id;
            bool is_selected = playlist_id == selected_playlist_id;
            int col = 0;
            
            ImGui::TableNextRow();
            
            if (is_playing) ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                                                   ImGui::GetColorU32(ImGuiCol_PlotLines));
            
            if (show_creator) {
                ImGui::TableSetColumnIndex(col++);
                ImGui::TextUnformatted(playlist.creator);
            }
            
            ImGui::TableSetColumnIndex(col++);
            if (show_playlist_selectable(playlist, is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                action->user_selected_playlist = true;
                action->selected_playlist_index = i;
            }
            //-
            // Drag-drop
            if (!no_edit && ImGui::BeginDragDropTarget()) {
                if (accept_drag_drop_to_playlist(playlist)) {
                    action->user_altered_playlist = true;
                    action->altered_playlist_index = i;
                }
                ImGui::EndDragDropTarget();
            }
            //-
            
            if (ImGui::IsItemClicked(ImGuiMouseButton_Middle) || is_item_double_clicked()) {
                action->user_requested_playlist = true;
                action->requested_playlist_index = i;
            }
            
            if (!no_edit && ImGui::BeginPopupContextItem()) {
                if (show_add_files_menu(&playlist)) {
                    action->user_altered_playlist = true;
                    action->altered_playlist_index = i;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Rename")) {
                    action->user_requested_rename_playlist = true;
                    action->requested_rename_playlist_index = i;
                }
                if (ImGui::MenuItem("Delete")) {
                    action->user_requested_delete_playlist = true;
                    action->requested_delete_playlist_index = i;
                }
                ImGui::EndPopup();
            }
            
            ImGui::TableSetColumnIndex(col++);
            ImGui::TextDisabled("%u", playlist.tracks.count);
        }
        
        ImGui::EndTable();
    }
}


struct Track_List_Column {
    const char *name;
    int sort_metric;
    ImGuiTableColumnFlags flags;
    float size;
};

enum {
    TRACK_COLUMN_TITLE,
    TRACK_COLUMN_ARTIST,
    TRACK_COLUMN_ALBUM,
    TRACK_COLUMN_DURATION,
};

const static Track_List_Column TRACK_COLUMNS[] = {
    {"Title", SORT_METRIC_TITLE, ImGuiTableColumnFlags_NoHide, 200.f},
    {"Artist", SORT_METRIC_ARTIST, 0, 150.f},
    {"Album", SORT_METRIC_ALBUM, 0, 150.f},
    {"Duration", SORT_METRIC_DURATION, 0, 150.f},
};

static void show_track_range(Playlist& playlist, u32 start, 
                             u32 end_exclusive, Track current_track, Track_List_Action *action, bool no_edit) {
    u32 playlist_id = playlist.get_id();
    bool want_remove = false;
    Metadata metadata;
            
    char filter_lowercase[FILTER_STRING_MAX];
    if (playlist.filter[0])
        string_to_lower(playlist.filter, filter_lowercase, sizeof(filter_lowercase));
    
    for (u32 i_track = start; i_track != end_exclusive; ++i_track) {
        const Track& track = playlist.tracks[i_track];
        const bool is_selected = is_track_selected(track);
        const bool is_playing = current_track == track;
        retrieve_metadata(track.metadata, &metadata);
        
        if (playlist.filter[0] && !metadata_meets_filter(metadata, filter_lowercase)) {
            continue;
        }
        
        ImGui::PushID((void*)(uintptr_t)i_track);
        
        ImGui::TableNextRow();
        
        //@NOTE: We don't use the PlotLines style color anywhere right now, so using it as
        // a highlighter for the current playing track/playlist seems appropriate
        if (is_playing) ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                                               ImGui::GetColorU32(ImGuiCol_PlotLines));
        
        if (ImGui::TableSetColumnIndex(TRACK_COLUMN_ALBUM)) {
            ImGui::TextUnformatted(metadata.album);
        }
        
        if (ImGui::TableSetColumnIndex(TRACK_COLUMN_ARTIST)) {
            ImGui::TextUnformatted(metadata.artist);
        }
        
        // Select track
        if (ImGui::TableSetColumnIndex(TRACK_COLUMN_TITLE)) {
            if (ImGui::Selectable(metadata.title, is_selected,
                                  ImGuiSelectableFlags_SpanAllColumns)) {
                select_track_in_playlist(playlist, i_track);
            }
        }
        
        // Drag-drop
        if (ImGui::BeginDragDropSource()) {
            begin_track_drag_drop();
            ImGui::EndDragDropSource();
        }
        
        // Play track
        if (ImGui::IsItemClicked(ImGuiMouseButton_Middle) || is_item_double_clicked()) {
            action->user_requested_track = true;
            action->requested_track_index = i_track;
            if (!is_selected) {
                clear_track_selection();
                select_track_in_playlist(playlist, i_track);
            }
        }
        
        // Context menu
        if (ImGui::BeginPopupContextItem()) {
            if (!is_selected) {
                clear_track_selection();
                select_track_in_playlist(playlist, i_track);
            }
            show_track_context_menu(playlist, i_track);
            
            if (!no_edit) {
                ImGui::Separator();
                if (ImGui::MenuItem("Remove")) want_remove = true;
            }
            
            ImGui::EndPopup();
        }
        
        // Duration
        if (ImGui::TableSetColumnIndex(TRACK_COLUMN_DURATION)) {
            ImGui::TextUnformatted(metadata.duration_string);
        }
        
        ImGui::PopID();
    }
    
    if (want_remove) {
        action->user_altered_playlist = true;
        remove_selected_tracks_from_playlist(playlist);
    }
}

static bool update_playlist_sort_specs(Playlist& playlist, ImGuiTableSortSpecs *spec) {
    if (spec->SpecsDirty) {
        const ImGuiTableColumnSortSpecs *col_sort = &spec->Specs[0];
        int metric = -1;
        int order = SORT_ORDER_DESCENDING;
        
        if (!col_sort) return false;
        spec->SpecsDirty = false;
        
        switch (col_sort->ColumnIndex) {
            case TRACK_COLUMN_TITLE: metric = SORT_METRIC_TITLE; break;
            case TRACK_COLUMN_ARTIST: metric = SORT_METRIC_ARTIST; break;
            case TRACK_COLUMN_ALBUM: metric = SORT_METRIC_ALBUM; break;
            case TRACK_COLUMN_DURATION: metric = SORT_METRIC_DURATION; break;
        }
        
        if (col_sort->SortDirection == ImGuiSortDirection_Ascending) {
            order = SORT_ORDER_ASCENDING;
        }
        
        ASSERT(metric != -1);
        if (metric >= 0) {
            sort_playlist(playlist, metric, order);
            return true;
        }
    }
    
    return false;
}

void show_playlist_track_list(const char *str_id, Playlist& playlist, Track current_track,
                              Track_List_Action *action, Track_List_Flags flags) {
    ImGuiTableFlags table_flags = 
        ImGuiTableFlags_BordersInner|ImGuiTableFlags_Resizable|
        ImGuiTableFlags_SizingFixedFit|ImGuiTableFlags_Hideable|ImGuiTableFlags_Reorderable|
        ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY;
    
    if (!(flags & TRACK_LIST_FLAGS_NO_FILTER)) {
        ImGui::InputTextWithHint("##filter", "Filter", playlist.filter, FILTER_STRING_MAX);
        ImGui::SameLine();
        if (ImGui::Button("Apply")) {
            action->want_apply_filter = true;
        }
    }
    
    const bool no_sort = (flags & TRACK_LIST_FLAGS_NO_SORT) != 0;
    if (!no_sort) table_flags |= ImGuiTableFlags_Sortable|ImGuiTableFlags_SortTristate;
    
    const ImGuiStyle& style = ImGui::GetStyle();
    const bool no_edit = (flags & TRACK_LIST_FLAGS_NO_EDIT) != 0;
    bool focused = false;
    
    if (ImGui::BeginTable(str_id, 4, table_flags)) {
        focused = ImGui::IsWindowFocused();
        
        // Set up columns
        for (auto col : TRACK_COLUMNS) {
            ImGuiTableColumnFlags flags = col.flags;
            if (!no_sort && col.sort_metric && (playlist.sort_metric == col.sort_metric)) {
                flags |= ImGuiTableColumnFlags_DefaultSort;
                if (playlist.sort_order == SORT_ORDER_DESCENDING)
                    flags |= ImGuiTableColumnFlags_PreferSortDescending;
                else
                    flags |= ImGuiTableColumnFlags_PreferSortAscending;
            }
            
            ImGui::TableSetupColumn(col.name, col.flags, col.size);
        }
        
        ImGui::TableSetupScrollFreeze(1, 1);
        ImGui::TableHeadersRow();
        
        if (playlist.filter[0]) {
            show_track_range(playlist, 0, playlist.tracks.count, current_track, action, no_edit);
        } else {
            // Cull out non-visible tracks
            ImGuiListClipper clipper = ImGuiListClipper();
            clipper.Begin(playlist.tracks.count);
            
            // Show visible tracks
            if (playlist.tracks.count) while (clipper.Step()) {
                show_track_range(playlist, clipper.DisplayStart, clipper.DisplayEnd, current_track, action, no_edit);
            }
        }
        
        // Sort the playlist if the user clicks on one of the headers
        ImGuiTableSortSpecs *sort_specs = ImGui::TableGetSortSpecs();
        if (sort_specs) 
            action->user_altered_playlist = update_playlist_sort_specs(playlist, sort_specs);
        
        ImGui::EndTable();
    }
    
    if (focused) {
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_A) && focused) {
            select_whole_playlist(playlist);
        }
    }
    
}

void show_detailed_metadata_table(const char *str_id, const Detailed_Metadata& metadata, Texture *cover_art) {
    ImGuiTableFlags table_flags = ImGuiTableFlags_RowBg;
    if (ImGui::BeginTable(str_id, 2, table_flags)) {
        ImGui::TableSetupColumn("##type", ImGuiTableColumnFlags_WidthStretch, 0.3f);
        ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch, 0.7f);
        
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Artist");
        ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(metadata.artist);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Album");
        ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(metadata.album);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Title");
        ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(metadata.title);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Genre");
        ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(metadata.genre);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Track Number");
        ImGui::TableSetColumnIndex(1); ImGui::Text("%u", metadata.track_number);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Year");
        ImGui::TableSetColumnIndex(1); ImGui::Text("%u", metadata.year);
        
        ImGui::EndTable();
    }
    
    if (cover_art) {
        //@TODO: Come up with some sizing rules for cover art
        const float size = ImGui::GetContentRegionAvail().x;
        ImGui::Image(cover_art, ImVec2(size, size));
    }
    
    if (metadata.comment[0]) {
        ImGui::SeparatorText("Comment");
        ImGui::TextWrapped("%s", metadata.comment);
    }
}

// Show menu items to add files or folders to a playlist
bool show_add_files_menu(Playlist *playlist) {
    Add_Tracks_Iterator_State iterator = {};
    iterator.target = playlist;
    
    if (ImGui::MenuItem("Add files")) {
        bool commit = open_file_multiselect_dialog(FILE_TYPE_AUDIO, &add_tracks_to_playlist_iterator,
                                                   &iterator);
        
        if (commit) {
            show_message_box(MESSAGE_BOX_TYPE_INFO, "Added %u tracks to %s",
                             iterator.track_count, iterator.target->name);
            iterator.target->sort();
            return true;
        }
    }
    
    if (ImGui::MenuItem("Add folders")) {
        bool commit = open_folder_multiselect_dialog(FILE_TYPE_AUDIO, &add_tracks_to_playlist_iterator, 
                                                     &iterator);
        
        if (commit) {
            show_message_box(MESSAGE_BOX_TYPE_INFO, "Added %u tracks to %s",
                             iterator.track_count, iterator.target->name);
            iterator.target->sort();
            return true;
        }
    }
    
    return false;
}

bool save_playlist_to_file(const Playlist& playlist, const wchar_t *filename) {
    FILE *f = _wfopen(filename, L"w");
    if (!f) {
        wlog_error(L"Failed to open file %s for writing\n", filename);
        return false;
    }
    defer(fclose(f));
    
    fprintf(f, "1\n"); // Version
    fprintf(f, "%s\n", playlist.name); // Name
    fprintf(f, "%s\n", sort_metric_to_string(playlist.sort_metric)); // Sort metric
    fprintf(f, "%s\n", sort_order_to_string(playlist.sort_order)); // Sort order
    
    // Track paths
    for (u32 i = 0; i < playlist.tracks.count; ++i) {
        char path[PATH_LENGTH];
        retrieve_file_path(playlist.tracks[i].path, path, PATH_LENGTH);
        fprintf(f, "%s\n", path);
    }
    
    return true;
}

#if 0
static size_t read_line(FILE *f, char *buffer, int buffer_size) {
    if (!fgets(buffer, buffer_size, f)) return 0;
    size_t length = strlen(buffer);
    if (buffer[length-1] == '\n') {
        buffer[length-1] = 0;
        return length-1;
    }
    else {
        return length;
    }
}

bool load_playlist_from_file(const wchar_t *path, Playlist& playlist) {
    FILE *f = _wfopen(path, L"r");
    if (!f) {
        wlog_error(L"Failed to open playlist %s for reading\n", path);
        return false;
    }
    defer(fclose(f));
    
    //setvbuf(f, NULL, _IOFBF, 16<<10);
    
    char line[1024];
    
    // Version
    if (read_line(f, line, sizeof(line)) == 0) return false;
    if (atoi(line) > 1) {
        log_error("Unsupported playlist file version %s\n", line);
        return false;
    }
    
    // Name
    if (read_line(f, line, sizeof(line)) == 0) return false;
    playlist.set_name(line);
    
    // Sort metric
    if (read_line(f, line, sizeof(line)) == 0) return false;
    playlist.sort_metric = sort_metric_from_string(line);
    
    // Sort order
    if (read_line(f, line, sizeof(line)) == 0) return false;
    playlist.sort_order = sort_order_from_string(line);
    
    // Tracks
    while (read_line(f, line, sizeof(line)) != 0) {
        wchar_t track_path[PATH_LENGTH];
        utf8_to_wchar(line, track_path, PATH_LENGTH);
        playlist.add_track(track_path);
    }
    
    return true;
}
#else

static INLINE bool iscontrol(int c) {
    return c == '\n' || c == '\r';
}

static bool read_line(char **memory, char *buffer, int buffer_size) {
    char *p = *memory;
    int i = 0;
    
    if (*p == 0) return false;
    
    while (*p && isspace(*p)) ++p;
    
    for (; *p && !iscontrol(*p) && (i < (buffer_size-1)); ++p, ++i) {
        buffer[i] = *p;
    }
    
    buffer[i] = 0;
    
    *memory = p;
    return true;
}

bool load_playlist_from_file(const wchar_t *path, Playlist& playlist) {
    char *buffer;
    char *f;
    if (!read_whole_file(path, (void**)&buffer, true)) return false;
    defer(free(buffer));
    
    f = buffer;
    
    char line[1024];
    
    // Version
    if (read_line(&f, line, sizeof(line)) == 0) return false;
    if (atoi(line) > 1) {
        log_error("Unsupported playlist file version %s\n", line);
        return false;
    }
    
    // Name
    if (read_line(&f, line, sizeof(line)) == 0) return false;
    playlist.set_name(line);
    
    // Sort metric
    if (read_line(&f, line, sizeof(line)) == 0) return false;
    playlist.sort_metric = sort_metric_from_string(line);
    
    // Sort order
    if (read_line(&f, line, sizeof(line)) == 0) return false;
    playlist.sort_order = sort_order_from_string(line);
    
    // Tracks
    while (read_line(&f, line, sizeof(line)) != 0) {
        wchar_t track_path[PATH_LENGTH];
        utf8_to_wchar(line, track_path, PATH_LENGTH);
        playlist.add_track(track_path);
    }
    
    return true;
}

#endif

