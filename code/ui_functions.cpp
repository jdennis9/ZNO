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
#include "theme.h"
#include <imgui.h>

bool show_playlist_selectable(const Playlist& playlist, bool playing, ImGuiSelectableFlags flags) {
    return ImGui::Selectable(playlist.name, playing, flags);
}

bool is_imgui_item_double_clicked() {
    return ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
}

void show_playlist_list(const char *str_id, Array_View<Playlist> playlists, 
                        u32 playing_playlist_id, Playlist_List_Action *action, Playlist_List_Flags flags,
                        u32 selected_playlist_id) {
    ImGuiTableFlags table_flags = ImGuiTableFlags_RowBg;
    *action = Playlist_List_Action{};
    
    bool show_creator = (flags & PLAYLIST_LIST_FLAGS_SHOW_CREATOR) != 0;
    bool no_edit = (flags & PLAYLIST_LIST_FLAGS_NO_EDIT);
    
    if (show_creator) 
        table_flags |= ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_Resizable;
    
    if (ImGui::BeginTable(str_id, 2 + show_creator, table_flags)) {
        //-
        // Setup columns
        if (show_creator) {
            ImGui::TableSetupColumn("No. Tracks", 0, 20.f);
            ImGui::TableSetupColumn("By", 0, 150.f);
            ImGui::TableSetupColumn("Title", 0, 150.f);
        }
        else {
            ImGui::TableSetupColumn("No. Tracks", ImGuiTableColumnFlags_WidthStretch, 0.15f);
            ImGui::TableSetupColumn("Title");
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
            
            ImGui::TableNextRow();
            
            if (is_playing) ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                                                   get_theme_color(THEME_COLOR_PLAYING_INDICATOR));
            
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%u", playlist.tracks.count);

            if (show_creator) {
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(playlist.creator);
            }
            
            ImGui::TableNextColumn();
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
            
            if (ImGui::IsItemClicked(ImGuiMouseButton_Middle) || is_imgui_item_double_clicked()) {
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
                             u32 end_exclusive, Track current_track, Track_List_Action *action, bool no_edit, bool scroll_to_current) {
    bool want_remove = false;
    Metadata metadata;
            
    char filter_lowercase[FILTER_STRING_MAX];
    if (playlist.filter[0])
        string_to_lower(playlist.filter, filter_lowercase, sizeof(filter_lowercase));
    
    for (u32 i_track = start; i_track != end_exclusive; ++i_track) {
        const Track& track = playlist.tracks[i_track];
        const bool is_selected = is_track_selected(track);
        const bool is_playing = current_track == track;

        library_get_track_metadata(track, &metadata);
        
        if (playlist.filter[0] && (!metadata_meets_filter(metadata, filter_lowercase)
            || !ImGui::IsRectVisible(ImVec2(1, ImGui::GetFrameHeightWithSpacing())))) {
            continue;
        }
        
        ImGui::PushID((void*)(uintptr_t)i_track);
        defer(ImGui::PopID());
        ImGui::TableNextRow();
        
        if (is_playing) {
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                                   get_theme_color(THEME_COLOR_PLAYING_INDICATOR));
        }
        
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

        // Scroll to track
        if (is_playing && scroll_to_current) {
            ImGui::SetScrollHereY();
        }
        
        // Drag-drop
        if (ImGui::BeginDragDropSource()) {
            begin_track_drag_drop();
            ImGui::EndDragDropSource();
        }
        
        // Play track
        if (ImGui::IsItemClicked(ImGuiMouseButton_Middle) || is_imgui_item_double_clicked()) {
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
    
    const bool no_edit = (flags & TRACK_LIST_FLAGS_NO_EDIT) != 0;
    bool focused = false;
    bool want_scroll_to_playing_track = false;
    i32 index_of_track_to_scroll_to = -1;
    
    if (ImGui::BeginTable(str_id, 4, table_flags)) {
        focused = ImGui::IsWindowFocused();
        
        if (focused) {
            if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_A)) {
                select_whole_playlist(playlist);
            }
            else if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Space)) {
                want_scroll_to_playing_track = true;
                index_of_track_to_scroll_to = playlist.index_of_track(current_track);
                if (index_of_track_to_scroll_to < 0) want_scroll_to_playing_track = false;
            }
        }

        // Set up columns
        for (const auto& col : TRACK_COLUMNS) {
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
            show_track_range(playlist, 0, playlist.tracks.count, current_track,
                action, no_edit, want_scroll_to_playing_track);
        } else {
            // Cull out non-visible tracks
            ImGuiListClipper clipper = ImGuiListClipper();
            clipper.Begin(playlist.tracks.count);
            
            if (want_scroll_to_playing_track)
                clipper.ForceDisplayRangeByIndices(index_of_track_to_scroll_to, index_of_track_to_scroll_to+1);

            // Show visible tracks
            if (playlist.tracks.count) while (clipper.Step()) {
                show_track_range(
                    playlist, clipper.DisplayStart, clipper.DisplayEnd,
                    current_track, action, no_edit, want_scroll_to_playing_track
                );
            }
        }
        
        // Sort the playlist if the user clicks on one of the headers
        ImGuiTableSortSpecs *sort_specs = ImGui::TableGetSortSpecs();
        if (sort_specs) 
            action->user_altered_playlist |= update_playlist_sort_specs(playlist, sort_specs);
        
        ImGui::EndTable();
    }

    
}

void show_detailed_metadata_table(const char *str_id, const Detailed_Metadata& metadata, Texture *cover_art) {
    ImGuiTableFlags table_flags = ImGuiTableFlags_RowBg;
    const float cover_size = ImGui::GetContentRegionAvail().x;

    if (cover_art) {
        //@TODO: Come up with some sizing rules for cover art
        ImGui::Image(cover_art, ImVec2(cover_size, cover_size));
    }
    else {
        ImGui::Dummy(ImVec2(cover_size, cover_size));
    }

    if (ImGui::BeginTable(str_id, 2, table_flags)) {
        ImGui::TableSetupColumn("##type", ImGuiTableColumnFlags_WidthStretch, 0.3f);
        ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch, 0.7f);
        
        if (metadata.album[0]) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Album");
            ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(metadata.album);
        }
        if (metadata.artist[0]) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Artist");
            ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(metadata.artist);
        }
        if (metadata.artist[0]) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Title");
            ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(metadata.title);
        }
        if (metadata.genre[0]) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Genre");
            ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(metadata.genre);
        }
        if (metadata.track_number != 0) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Track Number");
            ImGui::TableSetColumnIndex(1); ImGui::Text("%u", metadata.track_number);
        }
        if (metadata.year != 0) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Year");
            ImGui::TableSetColumnIndex(1); ImGui::Text("%u", metadata.year);
        }
        
        ImGui::EndTable();
    }
    
    if (metadata.comment[0]) {
        ImGui::SeparatorText("Comment");
        ImGui::TextWrapped("%s", metadata.comment);
    }
}

bool save_playlist_to_file(const Playlist& playlist, const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) {
        log_error("Failed to open file %s for writing\n", filename);
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
        library_get_track_path(playlist.tracks[i], path);
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
    
    for (; *p && !iscontrol(*p) && *p != '\n' && (i < (buffer_size-1)); ++p, ++i) {
        buffer[i] = *p;
    }
    
    buffer[i] = 0;
    
    *memory = p;
    return true;
}

bool load_playlist_from_file(const char *path, Playlist& playlist) {
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
        playlist.add_track(line);
    }
    
    return true;
}

#endif

