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
#include "defines.h"
#include "os.h"
#include "drag_drop.h"
#include "platform.h"
#include "playback.h"
#include "ui.h"
#include "main.h"
#include "video.h"
#include "theme.h"
#include "font_awesome.h"
#include "preferences.h"
#include "metadata.h"
#include "util.h"
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <locale.h>
#include <imgui.h>
#include <stb_image.h>

struct Main_Flags {
    bool reload_font;
};

struct Background {
    char path[512];
    Texture *texture;
    i32 width, height;
};

static float g_dpi_scale;
static bool g_obscured;
static Main_Flags g_flags;
static Background g_background;
static Preferences g_prefs;
static bool g_prefs_dirty;
static bool g_need_load_font;
static bool g_need_load_background;
static File_Drag_Drop_Payload g_drag_drop_payload;
static bool g_have_drag_drop_payload;
static bool g_drag_drop_done;

static void load_font(const char *path, int size, int icon_size, float dpi_scale);
static void set_background_image(const char *path);
static void render_background();
static void update_background();
static void update_font();


#ifdef DEF_WIN_MAIN
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
#else
int main(int argc, char *argv[])
#endif
{
    setlocale(LC_ALL, ".65001");
    srand((int)time(NULL));
    
    platform_init();
    playback_init();

    g_prefs.set_defaults();
    g_prefs.load_from_file(PLATFORM_PREFS_PATH);

    g_dpi_scale = platform_get_dpi_scale();

    //-
    // Initialize DirectX and ImGui
    START_TIMER(init_video, "Initialize DirectX and ImGui");
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    platform_init_imgui();
    STOP_TIMER(init_video);
    //-
    
    //-
    // Configure ImGui
    ImGuiIO& imgui_io = ImGui::GetIO();
    imgui_io.ConfigFlags |=
        ImGuiConfigFlags_NavEnableKeyboard|ImGuiConfigFlags_DockingEnable;
    //-
    
    //-
    // Load cached stuff. Needs to go before init_ui()
    load_metadata_cache(PLATFORM_METADATA_PATH);
    //-
    
    // Initialize UI before showing the window to avoid
    // flashbanging the user. Uses the metadata cache
    init_ui();
    
    //-
    // Load preferences and hotkeys
    apply_preferences();
    //-
    
    // Load font and background before showing the window
    update_background();
    update_font();
    
    platform_show_window(true);

    bool running = true;
    while (running) {
        platform_poll_events();

        if (g_prefs_dirty) {
            g_prefs.save_to_file(PLATFORM_PREFS_PATH);
        }
        
        // Load font and background if changed
        update_font();
        update_background();
        
        // Update UI
        if (video_begin_frame()) {
            ImGui::NewFrame();
        
            // This needs to be called within the ImGui frame
            if (g_have_drag_drop_payload) {
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceExtern)) {
                    ImGui::SetDragDropPayload("FILES", NULL, 0);
                    ImGui::SetTooltip("Drop files here");
                    ImGui::EndDragDropSource();
                }
            }
        
            show_ui();
            ImGui::EndFrame();
        
            if (g_drag_drop_done) {
                clear_file_drag_drop_payload();
                g_drag_drop_done = false;
            }
        
            render_background();
            ImGui::Render();

            g_obscured = !video_end_frame();
        }
    }
    
    g_prefs.save_to_file(PLATFORM_PREFS_PATH);
    save_metadata_cache(PLATFORM_METADATA_PATH);
    destroy_texture(&g_background.texture);
    platform_deinit();
    
    return 0;
}

void notify(int message) {
    platform_notify(message);
}

static void render_background() {
    // Render background
    if (g_background.texture) {
        ImDrawList *drawlist = ImGui::GetBackgroundDrawList(ImGui::GetMainViewport());
        
        int width = g_background.width;
        int height = g_background.height;
        int winwidth;
        int winheight;

        platform_get_window_size(&winwidth, &winheight);
        
        if ((height < winheight) || (height > winheight)) {
            float ratio = (float)winheight / (float)height;
            width = (int)(width * ratio);
            height = (int)(height * ratio);
        }
        
        if (width < winwidth) {
            float ratio = (float)winwidth / (float)width;
            width = (int)(width * ratio);
            height = (int)(height * ratio);
        }
        
        ImVec2 min = ImVec2(0, 0);
        ImVec2 max = ImVec2((float)width, (float)height);
        
        drawlist->AddImage(g_background.texture, min, max, ImVec2(0, 0), ImVec2(1, 1));
    }

}

static void set_background_image(const char *path) {
    Image image;
    
    if (!path) {
        memset(g_background.path, 0, sizeof(g_background.path));
        if (g_background.texture) destroy_texture(&g_background.texture);
        return;
    }
    
    // Check if background is already loaded
    if (!strcmp(path, g_background.path)) return;
    
    if (!load_image_from_file(path, &image)) {
        memset(g_background.path, 0, sizeof(g_background.path));
        return;
    }
    
    strncpy0(g_background.path, path, sizeof(g_background.path)-1);
    
    if (g_background.texture) destroy_texture(&g_background.texture);
    
    g_background.texture = create_texture_from_image(&image);
    g_background.width = image.width;
    g_background.height = image.height;
    
    free_image(&image);
}

// Pass in NULL to load default font
static void load_font(const char *path, int size, int icon_size, float scale) {
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig cfg = ImFontConfig();
    bool load_default = false;
    float scaled_font_size = MAX(size*scale, 8.f);
    float scaled_icon_size = MAX(icon_size*scale, 8.f);
    
    cfg.RasterizerDensity = scale;

    video_invalidate_imgui_objects();
    
    io.Fonts->Clear();
    
    if (path) {
        ImFontGlyphRangesBuilder builder = {};
        static ImVector<ImWchar> builder_output;
        builder_output.clear();
        const static ImWchar ranges[] = {
            0x2010, 0x2015, // Dashes/hyphens
            0x2018, 0x201f, // Quotation marks
            0x2070, 0x207f, // Superscripts
            0x2080, 0x208e, // Subscripts
            0x2160, 0x217f, // Roman numerals
            0x2145, 0x2149, // Some Italic characters
            0x2100, 0x2134, // Letter-like symbols
            0,
        };

        builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
        builder.AddRanges(io.Fonts->GetGlyphRangesCyrillic());
        builder.AddRanges(io.Fonts->GetGlyphRangesJapanese());
        builder.AddRanges(io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
        builder.AddRanges(io.Fonts->GetGlyphRangesKorean());
        builder.AddRanges(io.Fonts->GetGlyphRangesThai());
        builder.AddRanges(io.Fonts->GetGlyphRangesVietnamese());
        builder.AddRanges(io.Fonts->GetGlyphRangesGreek());
        builder.AddRanges(ranges);
        builder.BuildRanges(&builder_output);
        io.Fonts->AddFontFromFileTTF(path, scaled_font_size, &cfg, builder_output.Data);
    } else load_default = true;
    
    if (load_default) {
        if (path) log_warning("Failed to load font %s, using fallback\n", path);
        io.Fonts->AddFontDefault();
    }
    
    static const ImWchar icon_ranges[] = {
        0xf048, 0xf052, // playback control icons
        0xf026, 0xf028, // volume icons
        0xf074, 0xf074, // shuffle icon
        0
    };
    
    cfg.FontDataOwnedByAtlas = false; // Tell ImGui to not try to free the data after use
    cfg.MergeMode = true;
    io.Fonts->AddFontFromMemoryTTF(FONT_AWESOME_OTF, FONT_AWESOME_OTF_SIZE,
                                   scaled_icon_size, &cfg, icon_ranges);
    
    video_create_imgui_objects();
}

static void update_background() {
    if (g_need_load_background) {
        START_TIMER(load_bg, "Load background");
        set_background_image(g_prefs.background[0] ? g_prefs.background : NULL);
        g_need_load_background = false;
        STOP_TIMER(load_bg);
    }
}

static void update_font() {
    if (g_need_load_font) {
        START_TIMER(load_font, "Load font");
        load_font(g_prefs.font[0] ? g_prefs.font : NULL, g_prefs.font_size, 
                  g_prefs.icon_font_size, g_dpi_scale);
        g_need_load_font = false;
        STOP_TIMER(load_font);
    }
}

bool load_image_from_file(const char *filename, Image *image) {
    image->data = stbi_load(filename, &image->width, &image->height, NULL, 4);
    if (!image->data) return false;
    image->format = IMAGE_FORMAT_R8G8B8A8;
    return true;
}

bool load_image_from_memory(const void *data, u32 data_size, Image *image) {
    image->data = stbi_load_from_memory((const stbi_uc*)data, data_size, &image->width, &image->height, NULL, 4);
    if (!image->data) return false;
    image->format = IMAGE_FORMAT_R8G8B8A8;
    return true;
}

void free_image(Image *image) {
    if (image->data) stbi_image_free(image->data);
}


Preferences& get_preferences() {
    return g_prefs;
}

void set_preferences_dirty() {
    g_prefs_dirty = true;
}

void apply_preferences() {
    const Preferences& prefs = g_prefs;
    g_need_load_background = true;
    g_need_load_font = true;
    load_theme(prefs.theme);
    g_prefs.save_to_file(PLATFORM_PREFS_PATH);
    platform_apply_preferences();
}


void set_window_title_message(const char *format, ...) {
    wchar_t title[512] = {};
    char formatted_message[512];
    va_list va;
    va_start(va, format);
    vsnprintf(formatted_message, sizeof(formatted_message)-1, format, va);
    va_end(va);
    
    platform_set_window_title(formatted_message);
}

//-
// Drag drop
void tell_main_we_have_a_drag_drop_payload() {
    g_have_drag_drop_payload = true;
}

void tell_main_weve_dropped_the_drag_drop_payload() {
    g_drag_drop_done = true;
}

void add_to_file_drag_drop_payload(const char *path) {
    g_drag_drop_payload.
        offsets.append(g_drag_drop_payload.string_pool.append_array(path, (u32)strlen(path)+1));
}

void clear_file_drag_drop_payload() {
    g_have_drag_drop_payload = false;
    g_drag_drop_payload.string_pool.free();
    g_drag_drop_payload.offsets.free();
}

const File_Drag_Drop_Payload& get_file_drag_drop_payload() {
    return g_drag_drop_payload;
}

//-

