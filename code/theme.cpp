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
#include "theme.h"
#include "platform.h"
#include "main.h"
#include "filenames.h"
#include <imgui.h>
#include <ini.h>
#include <assert.h>

struct Font_Name {
    char name[128];
};

struct Theme {
    char name[MAX_THEME_NAME_LENGTH+1];
};

static ImVec4 g_theme_colors[THEME_COLOR__COUNT];

static const struct Color_Info {
    int color;
    const char *name;
    const char *ini_name;
    u32 def;
} g_color_info[] = {
    {THEME_COLOR_PLAYING_INDICATOR, "Playing Indicator", "PlayingIndicator"},
    {THEME_COLOR_PEAK_METER, "Peak Meter", "PeakMeter"},
    {THEME_COLOR_PEAK_METER_BG, "Peak Meter Bg.", "PeakMeterBg"},
};

static Array<Theme> g_themes;
static u32 g_selected_theme;
static char g_loaded_theme_name[MAX_THEME_NAME_LENGTH+1];

static u32 flip_endian(u32 v) {
    u32 ret;
    char *i = (char*)&v;
    char *o = (char*)&ret;
    o[0] = i[3];
    o[1] = i[2];
    o[2] = i[1];
    o[3] = i[0];
    return ret;
}

static int theme_ini_handler(void *data, const char *section, const char *key, const char *value) {
    ImGuiStyle& style = ImGui::GetStyle();
    
    if (!strcmp(section, "ImGuiColors")) {
        for (u32 i = 0; i < ImGuiCol_COUNT; ++i) {
            const char *name = ImGui::GetStyleColorName(i);
            if (!strcmp(key, name)) {
                u32 color = (u32)strtoll(value, NULL, 16);
                color = flip_endian(color);
                style.Colors[i] = ImColor(color).Value;
            }
        }
    }
    else if (!strcmp(section, "InternalColors")) {
        for (u32 i = 0; i < ARRAY_LENGTH(g_color_info); ++i) {
            const Color_Info& info = g_color_info[i];
            
            if (!strcmp(key, info.ini_name)) {
                u32 color = (u32)strtoll(value, NULL, 16);
                color = flip_endian(color);
                g_theme_colors[info.color] = ImColor(color).Value;
                break;
            }
        }
    }
    
    return true;
}

static Recurse_Command add_theme_from_dir(void *dont_care, const wchar_t *path_u16, bool is_folder) {
    if (is_folder) return RECURSE_CONTINUE;
    char path[512];
    wchar_to_utf8(path_u16, path, sizeof(path));
    
    const char *filename = get_file_name(path);
    u32 length = get_file_name_length_without_extension(path);
    Theme name = {};
    assert(length < MAX_THEME_NAME_LENGTH);
    if (length == 0) return RECURSE_CONTINUE;
    strncpy(name.name, filename, length);
    name.name[length] = 0;
    g_themes.append(name);
    
    return RECURSE_CONTINUE;
}


static void refresh_themes() {
    g_themes.clear();
    for_each_file_in_folder(L"Themes\\", &add_theme_from_dir, NULL);
}

void set_default_theme() {
    g_theme_colors[THEME_COLOR_PLAYING_INDICATOR] = ImColor(0xff0074ff).Value;
    g_theme_colors[THEME_COLOR_PEAK_METER] = ImColor(0xff00ff00).Value;
    g_theme_colors[THEME_COLOR_PEAK_METER_BG] = ImColor(0xff000000).Value;
    refresh_themes();
}

static u32 get_theme_index(const char *name) {
    for (u32 i = 0; i < g_themes.count; ++i) {
        if (!strcmp(g_themes[i].name, name)) return i;
    }
    return UINT32_MAX;
}

void load_theme(const char *name) {
    ImGuiStyle& style = ImGui::GetStyle();
    refresh_themes();
    g_selected_theme = get_theme_index(name);
    
    if (g_selected_theme == UINT32_MAX) {
        g_selected_theme = 0;
        log_debug("Couldn't find theme \"%s\"\n", name);
        return;
    }
    
    style = ImGuiStyle();
    ImGui::StyleColorsDark();
    
    set_default_theme();
    
    char path[256];
    snprintf(path, 256, "Themes\\%s.ini", name);
    ini_parse(path, &theme_ini_handler, NULL);
    
    style.SeparatorTextBorderSize = 1.f;
    strncpy0(g_loaded_theme_name, name, sizeof(g_loaded_theme_name));
}

void save_theme(const char *name) {
    char path[256];
    
    if (!does_file_exist(L"Themes")) {
        create_directory(L"Themes");
    }
    
    g_selected_theme = get_theme_index(name);
    if (g_selected_theme == UINT32_MAX) {
        Theme new_theme = {};
        strncpy(new_theme.name, name, MAX_THEME_NAME_LENGTH);
        g_selected_theme = g_themes.append(new_theme);
    }
    
    const Theme& theme = g_themes[g_selected_theme];
    ImGuiStyle& style = ImGui::GetStyle();
    
    snprintf(path, 256, "Themes\\%s.ini", theme.name);
    
    FILE *file = fopen(path, "w");
    if (!file) return;
    
    fprintf(file, "[InternalColors]\n");
    for (u32 i = 0; i < ARRAY_LENGTH(g_color_info); ++i) {
        const Color_Info& info = g_color_info[i];
        u32 color;
        color = ImGui::GetColorU32(g_theme_colors[info.color]);
        color = flip_endian(color);
        fprintf(file, "%s = %x\n", info.ini_name, color);
    }
    
    fprintf(file, "[ImGuiColors]\n");
    for (u32 i = 0; i < ImGuiCol_COUNT; ++i) {
        u32 color = ImGui::GetColorU32(style.Colors[i]);
        color = flip_endian(color);
        fprintf(file, "%s = %x\n", ImGui::GetStyleColorName(i), color);
    }
    
    fclose(file);
}

bool show_theme_editor_gui() {
    ImGuiStyle& style = ImGui::GetStyle();
    static char theme_name[MAX_THEME_NAME_LENGTH];
    //static const char *editing_theme;
    static bool new_theme = false;
    static bool dirty = false;
    
    if (ImGui::InputText("##name", theme_name, MAX_THEME_NAME_LENGTH)) new_theme = true;
    
    if (!new_theme) {
        const char *editing_theme = get_loaded_theme();
        if (editing_theme) {
            strcpy(theme_name, editing_theme);
        }
    } else dirty = true;
    
    ImGui::SameLine();
    if (ImGui::BeginCombo("##select_theme", "", ImGuiComboFlags_NoPreview)) {
        const char *sel = show_theme_selector_gui();
        if (sel) {
            load_theme(sel);
            strcpy(theme_name, sel);
            new_theme = false;
            dirty = false;
        }
        
        ImGui::EndCombo();
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        if (theme_name[0]) {
            bool confirm = true;
            if (!new_theme) {
                confirm = show_confirm_dialog("Confirm Overwrite Theme", "Overwrite theme '%s'?", theme_name);
            }
            if (confirm) {
                save_theme(theme_name);
                dirty = false;
            }
            new_theme = false;
        }
        else {
            show_message_box(MESSAGE_BOX_TYPE_WARNING, "Cannot create theme with an empty name.");
        }
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Load")) {
        load_theme(theme_name);
    }
    
    ImGui::SeparatorText("ZNO Colors");
    for (u32 i = 0; i < ARRAY_LENGTH(g_color_info); ++i) {
        int index = g_color_info[i].color;
        dirty |= ImGui::ColorEdit4(g_color_info[i].name, &g_theme_colors[index].x);
    }
    
    ImGui::SeparatorText("ImGui Colors");
    if (ImGui::Button("Set to default light theme")) {
        const char *message = 
            "Reset all ImGui colors to the default light theme? This cannot be undone.";
        if (show_confirm_dialog("Confirm Reset Colors", message)) {
            ImGui::StyleColorsLight();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Set to default dark theme")) {
        const char *message = 
            "Reset all ImGui colors to the default dark theme? This cannot be undone.";
        if (show_confirm_dialog("Confirm Reset Colors", message)) {
            ImGui::StyleColorsDark();
        }
    }
    
    for (u32 i = 0; i < ImGuiCol_COUNT; ++i) {
        ImGuiCol_ color_idx = (ImGuiCol_)i;
        const char *name = ImGui::GetStyleColorName(color_idx);
        dirty |= ImGui::ColorEdit4(name, &style.Colors[i].x);
    }
    
    return dirty;
}

u32 get_theme_color(Theme_Color color) {
    u32 c = ImGui::GetColorU32(g_theme_colors[color]);
    return c;
}

const char *show_theme_selector_gui() {
    if (g_themes.count) {
        for (u32 i = 0; i < g_themes.count; ++i) {
            if (ImGui::Selectable(g_themes[i].name)) {
                return g_themes[i].name;
            }
        }
    }
    else {
        ImGui::TextDisabled("No themes found");
    }
    
    return NULL;
}

const char *get_loaded_theme() {
    u32 index = get_theme_index(g_loaded_theme_name);
    if (index == UINT32_MAX) return NULL;
    return g_themes[index].name;
}