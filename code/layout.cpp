#include "layout.h"
#include "builtin_layouts.h"
#include "platform.h"
#include "array.h"
#include "filenames.h"
#include <imgui.h>

#define LAYOUTS_PATH L"Layouts"

struct Layout {
    char name[64];
    const char *ini;
};

struct Custom_Layout {
    char name[64];
};

static Layout g_builtin_layouts[] = {
    {"Default", DEFAULT_LAYOUT_INI},
    {"Theme Editing", THEME_EDITING_LAYOUT_INI},
    {"Minimal", MINIMAL_LAYOUT_INI},
};

static Array<Custom_Layout> g_custom_layouts;

void layout_init() {
    if (!does_file_exist(L"imgui.ini")) {
        ImGui::LoadIniSettingsFromMemory(DEFAULT_LAYOUT_INI);
    }

    if (!does_file_exist(LAYOUTS_PATH)) {
        create_directory(LAYOUTS_PATH);
    }

    auto iterator = [](void *dont_care, const wchar_t *wpath, bool is_folder) -> Recurse_Command {
        if (is_folder) return RECURSE_CONTINUE;
        char path[PATH_LENGTH];
        Custom_Layout layout = {};
        u32 length_without_extension;
        const char *filename;
        wchar_to_utf8(wpath, path, PATH_LENGTH);

        filename = get_file_name(path);
        length_without_extension = get_file_name_length_without_extension(filename);
        if (length_without_extension > sizeof(layout.name)) return RECURSE_CONTINUE;
        memcpy(layout.name, filename, length_without_extension);

        g_custom_layouts.append(layout);

        return RECURSE_CONTINUE;
    };

    for_each_file_in_folder(LAYOUTS_PATH, iterator, NULL);
}

void layout_show_selector() {
    for (const auto& layout : g_builtin_layouts) {
        if (ImGui::MenuItem(layout.name)) {
            ImGui::LoadIniSettingsFromMemory(layout.ini);
        }
    }

    if (g_custom_layouts.count) ImGui::Separator();

    for (const auto& layout : g_custom_layouts) {
        if (ImGui::MenuItem(layout.name)) {
            char path[PATH_LENGTH] = {};
            snprintf(path, sizeof(path)-1, "%ls/%s.ini", LAYOUTS_PATH, layout.name);
            ImGui::LoadIniSettingsFromDisk(path);
        }
    }
}

void layout_show_deleter() {
    i32 delete_index = -1;

    for (u32 i = 0; i < g_custom_layouts.count; ++i) {
        const auto& layout = g_custom_layouts[i];
        if (ImGui::MenuItem(layout.name)) {
            delete_index = i;
        }
    }

    if (delete_index >= 0) {
        Custom_Layout layout = g_custom_layouts[delete_index];
        wchar_t path[PATH_LENGTH] = {};
        _snwprintf(path, PATH_LENGTH-1, L"%s/%hs.ini", LAYOUTS_PATH, layout.name);
        delete_file(path);
    }
}

const char *layout_show_custom_name_selecor() {
    for (const auto& layout : g_custom_layouts) {
        if (ImGui::MenuItem(layout.name)) {
            return layout.name;
        }
    }

    return NULL;
}

void layout_save_current(const char *name) {
    u32 index = g_custom_layouts.push();
    strncpy0(g_custom_layouts[index].name, name, sizeof(g_custom_layouts[index].name));
    layout_overwrite_with_current(index);
}

void layout_overwrite_with_current(i32 index) {
    Custom_Layout& layout = g_custom_layouts[index];
    char path[PATH_LENGTH] = {};
    snprintf(path, PATH_LENGTH-1, "%ls/%s.ini", LAYOUTS_PATH, layout.name);
    ImGui::SaveIniSettingsToDisk(path);
}

i32 layout_get_index_from_name(const char *name) {
    for (u32 i = 0; i < g_custom_layouts.count; ++i) {
        if (!strcmp(name, g_custom_layouts[i].name)) {
            return i;
        }
    }

    return -1;
}
