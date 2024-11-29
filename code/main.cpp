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
#include "platform.h"
#include "playback.h"
#include "ui.h"
#include "main.h"
#include "video.h"
#include "theme.h"
#include "font_awesome.h"
#include "preferences.h"
#include "metadata.h"
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <locale.h>
#include <windows.h>
//--------------------
// For dark title bar
#include <dwmapi.h>
#include <windowsx.h>
//--------------------
#include <imgui.h>
#include <d3d10.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx10.h>
#include <stb_image.h>

#include "drag_drop.h"
#include "media_controls.h"

#define WINDOW_CLASS_NAME L"ZNO_WINDOW_CLASS"
#define WINDOW_TITLE (L"ZNO MP " APP_VERSION_STRING)
#define PREFS_PATH "Preferences.ini"
#define HOTKEYS_PATH "Hotkeys.ini"
#define METADATA_CACHE_PATH L"metadata.dat"

struct Window {
    int resize_width;
    int resize_height;
    int width;
    int height;
    float dpi_scale;
    bool is_obscured;
};

struct Main_Flags {
    bool reload_font;
};

struct D3D {
    ID3D10Device *device;
    IDXGISwapChain *swapchain;
    ID3D10RenderTargetView *render_target;
};

struct Background {
    char path[512];
    Texture *texture;
    i32 width, height;
};

struct Hotkey {
    UINT mods;
    UINT vk;
};

static HWND g_hwnd;
static Window g_window;
static Main_Flags g_flags;
static D3D d3d;
static Background g_background;
static Hotkey g_hotkeys[HOTKEY__COUNT];
static bool g_hotkey_is_bound[HOTKEY__COUNT];
static Preferences g_prefs;
static bool g_prefs_dirty;
static bool g_need_load_font;
static bool g_need_load_background;
static bool g_capture_next_input;
static int g_capture_input_target;
static HMENU g_tray_popup;
static HICON g_icon;
static File_Drag_Drop_Payload g_drag_drop_payload;
static bool g_have_drag_drop_payload;
static bool g_drag_drop_done;

IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT WINAPI window_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static bool create_d3d_device();
static void resize_swapchain();
static void render_frame();
static void load_font(const char *path, int size, int icon_size, float dpi_scale);
static bool is_window_visible();
static void set_background_image(const char *path);
static bool is_key_down(int vk) {return (GetKeyState(vk) * 0x8000) != 0;}
static void load_hotkeys();
static void apply_hotkeys();
static void save_preferences();
static void load_preferences();
static void create_tray_icon();
static void remove_tray_icon();
static void update_background();
static void update_font();

#ifdef DEF_WIN_MAIN
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
#else
int main(int argc, char *argv[])
#endif
{
    
#ifndef DEF_WIN_MAIN
    HINSTANCE hInstance = GetModuleHandle(NULL);
#endif


    ImGui_ImplWin32_EnableDpiAwareness();
    (void)OleInitialize(NULL);
    (void)CoInitializeEx(NULL, COINIT_MULTITHREADED);
    setlocale(LC_ALL, ".65001");
    srand((int)time(NULL));
    g_prefs.set_defaults();
    g_prefs.load_from_file(PREFS_PATH);
    g_icon = LoadIconA(hInstance, "WindowIcon");
    
    playback_init();
    init_platform();
    install_media_controls_handler();
    
    // Register window class
    {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_OWNDC;
        wc.lpfnWndProc = &window_proc;
        wc.lpszClassName = WINDOW_CLASS_NAME;
        wc.hInstance = hInstance;
        wc.hIcon = g_icon;
        RegisterClassExW(&wc);
    }
    
    // Create window
    g_hwnd = CreateWindowExW(WS_EX_ACCEPTFILES,
                             WINDOW_CLASS_NAME,
                             WINDOW_TITLE,
                             WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT,
                             CW_USEDEFAULT,
                             CW_USEDEFAULT,
                             CW_USEDEFAULT,
                             NULL,
                             NULL,
                             hInstance,
                             NULL);
    // Set dark title bar
	{
		BOOL on = TRUE;
		DwmSetWindowAttribute(g_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &on, sizeof(on));
    }
    
    UpdateWindow(g_hwnd);

    g_window.dpi_scale = ImGui_ImplWin32_GetDpiScaleForHwnd(g_hwnd);

    //-
    // Initialize DirectX and ImGui
    START_TIMER(init_video, "Initialize DirectX10 and ImGui");
    create_d3d_device();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX10_Init(d3d.device);
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
    load_metadata_cache(METADATA_CACHE_PATH);
    //-
    
    // Initialize UI before showing the window to avoid
    // flashbanging the user. Uses the metadata cache
    init_ui();
    
    //-
    // Load preferences and hotkeys
    apply_preferences();
    load_hotkeys();
    apply_hotkeys();
    //-
    
    // Load font and background before showing the window
    update_background();
    update_font();

    create_tray_icon();
    init_drag_drop(g_hwnd);
    
    ShowWindow(g_hwnd, SW_NORMAL);


    
    bool running = true;
    while (running) {
        MSG msg;
        
        if (IsWindowVisible(g_hwnd)) {
            while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
                if (msg.message == WM_QUIT) {
                    log_debug("Received WM_QUIT\n");
                    running = false;
                    break;
                }
            }
        }
        else {
            GetMessageA(&msg, NULL, 0, 0);
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) {
                running = false;
                log_debug("Received WM_QUIT\n");
            }
            continue;
        }

        if (g_prefs_dirty) {
            g_prefs.save_to_file(PREFS_PATH);
        }
        
        if (!running) break;
        
        // Window is minimized
        if (g_window.is_obscured) Sleep(20);
        
        // Resize window
        if (g_window.resize_width != 0) resize_swapchain();
        
        // Load font and background if changed
        update_font();
        update_background();
        
        // Update UI
        ImGui_ImplDX10_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        
        // This needs to be called within ImGui::NewFrame
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
        
        render_frame();
    }
    
    ImGui_ImplWin32_Shutdown();
    ImGui_ImplDX10_Shutdown();
    
    g_prefs.save_to_file(PREFS_PATH);
    remove_tray_icon();
    save_metadata_cache(METADATA_CACHE_PATH);
    destroy_texture(&g_background.texture);
    d3d.device->Release();
    
	return 0;
}

static void unregister_hotkeys() {
    for (u32 i = 0; i < HOTKEY__COUNT; ++i) {
        if (g_hotkey_is_bound[i]) {
            UnregisterHotKey(g_hwnd, i);
            g_hotkey_is_bound[i] = false;
        }
    }
}

const char *get_hotkey_name(int hotkey) {
    switch (hotkey) {
        case HOTKEY_PREV_TRACK: return "PrevTrack";
        case HOTKEY_NEXT_TRACK: return "NextTrack";
        case HOTKEY_TOGGLE_PLAYBACK: return "TogglePlayback";
    }
    ASSERT(false && "Forgot to add hotkey name");
    return NULL;
}

static void load_hotkeys() {
    for (u32 i = 0; i < HOTKEY__COUNT; ++i) {
        if (g_hotkey_is_bound[i]) {
            UnregisterHotKey(g_hwnd, i);
            log_debug("Unregister hot key %d\n", i);
            g_hotkey_is_bound[i] = false;
        }
    }
    
    auto callback =
    [](void *dont_care, const char *section, const char *key, const char *value) -> int {
        for (int i = 0; i < HOTKEY__COUNT; ++i) {
            const char *hk_name = get_hotkey_name(i);
            char mods_name[64];
            char key_name[64];
            snprintf(mods_name, sizeof(mods_name), "%sMods", hk_name);
            snprintf(key_name, sizeof(key_name), "%sKey", hk_name);
            
            if (!strcmp(key, mods_name)) {
                g_hotkey_is_bound[i] = true;
                g_hotkeys[i].mods = (UINT)strtoll(value, NULL, 16);
            }
            else if (!strcmp(key, key_name)) {
                g_hotkey_is_bound[i] = true;
                g_hotkeys[i].vk = (UINT)strtoll(value, NULL, 16);
            }
        }
        
        return 1;
    };
    
    ini_parse(HOTKEYS_PATH, callback, NULL);
}

static void save_hotkeys() {
    FILE *f = fopen(HOTKEYS_PATH, "w");
    if (!f) return;
    
    fprintf(f, "[Hotkeys]\n");
    
    for (int i = 0; i < HOTKEY__COUNT; ++i) {
        if (!g_hotkey_is_bound[i]) continue;
        Hotkey hk = g_hotkeys[i];
        const char *hk_name = get_hotkey_name(i);
        char mods_name[64];
        char key_name[64];
        snprintf(mods_name, sizeof(mods_name), "%sMods", hk_name);
        snprintf(key_name, sizeof(key_name), "%sKey", hk_name);
        
        fprintf(f, "%s = %x\n", mods_name, hk.mods);
        fprintf(f, "%s = %x\n", key_name, hk.vk);
    }
    
    fclose(f);
}

static void apply_hotkeys() {
    for (int i = 0; i < HOTKEY__COUNT; ++i) {
        if (g_hotkey_is_bound[i]) {
            log_debug("Register hotkey %d\n", i);
            Hotkey hk = g_hotkeys[i];
            UnregisterHotKey(g_hwnd, i);
            if (!RegisterHotKey(g_hwnd, i, hk.mods|MOD_NOREPEAT, hk.vk)) {
                log_warning("Could not register hotkey %s\n", get_hotkey_name(i));
            }
            g_hotkey_is_bound[i] = true;
        }
    }
    
    save_hotkeys();
}

void notify(int message) {
    PostMessageW(g_hwnd, WM_USER+message, 0, 0);
}

f32 get_dpi_scale() {
    return g_window.dpi_scale;
}

static LRESULT WINAPI window_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;
    
    // Capture next input and bind it to a hotkey
    if (g_capture_next_input && (msg == WM_KEYDOWN)) {
        WPARAM vk = wParam;
        UINT mods = 0;
        if (ImGui::IsKeyDown(ImGuiMod_Ctrl)) mods |= MOD_CONTROL;
        if (ImGui::IsKeyDown(ImGuiMod_Shift)) mods |= MOD_SHIFT;
        if (ImGui::IsKeyDown(ImGuiMod_Alt)) mods |= MOD_ALT;
        
        if (vk == VK_ESCAPE) {
            g_capture_next_input = false;
            apply_hotkeys();
        }
        else if (vk != VK_SHIFT && vk != VK_CONTROL && vk != VK_MENU) {
            g_capture_next_input = false;
            g_hotkey_is_bound[g_capture_input_target] = true;
            g_hotkeys[g_capture_input_target].mods = mods;
            g_hotkeys[g_capture_input_target].vk = (UINT)vk;
            apply_hotkeys();
        }
    }
	
	switch (msg) {
		case WM_SIZE: {
			g_window.resize_width = LOWORD(lParam);
			g_window.resize_height = HIWORD(lParam);
			g_window.width = g_window.resize_width;
			g_window.height = g_window.resize_height;
			return 0;
		}
        
		case WM_GETMINMAXINFO: {
			LPMINMAXINFO info = (LPMINMAXINFO)lParam;
			info->ptMinTrackSize.x = 500;
			info->ptMinTrackSize.y = 500;
			break;
		}
        
		case WM_CLOSE: {
            switch (g_prefs.close_policy) {
                case CLOSE_POLICY_ALWAYS_ASK:
                if (MessageBoxA(NULL, "Minimize to tray?", "Closing Player", MB_YESNO | MB_ICONQUESTION) == IDYES)
                    ShowWindow(hWnd, SW_HIDE);
                else
                    PostQuitMessage(0);
                break;
                case CLOSE_POLICY_MINIMIZE_TO_TRAY:
                notify(NOTIFY_MINIMIZE_TO_TRAY);
                break;
                case CLOSE_POLICY_EXIT:
                PostQuitMessage(0);
                break;
            }
			return 0;
		}
        
        // Handle events from tray icon
        case WM_APP + 1: {
            // User clicked on tray icon, bring the window to front
			if (LOWORD(lParam) == WM_LBUTTONDOWN) {
				ShowWindow(hWnd, SW_SHOW);
				SetForegroundWindow(hWnd);
			}
			else if (LOWORD(lParam) == WM_RBUTTONDOWN) {
				POINT mouse;
				GetCursorPos(&mouse);
				TrackPopupMenuEx(g_tray_popup, TPM_LEFTBUTTON, mouse.x, mouse.y, hWnd, NULL);
				PostMessage(hWnd, WM_NULL, 0, 0);
			}
			return 0;
		}
        
        // Handle context menu from tray icon
        case WM_COMMAND: {
            if (wParam == 1) {
                PostQuitMessage(0);
            }
            return 0;
        }
        
		case WM_DPICHANGED: {
			g_window.dpi_scale = ImGui_ImplWin32_GetDpiScaleForHwnd(g_hwnd);
			g_flags.reload_font = true;
			return 0;
		}
        
        case WM_HOTKEY: {
            log_debug("WM_HOTKEY: %lld\n", wParam);
            switch (wParam) {
                case HOTKEY_TOGGLE_PLAYBACK:
                playback_toggle();
                break;
                case HOTKEY_NEXT_TRACK:
                ui_play_next_track();
                break;
                case HOTKEY_PREV_TRACK:
                ui_play_previous_track();
                break;
            }
            return 0;
        }

        
        case WM_USER+NOTIFY_QUIT: {
			PostQuitMessage(0);
            return 0;
        }
        
        case WM_USER+NOTIFY_MINIMIZE_TO_TRAY: {
            ShowWindow(hWnd, SW_HIDE);
            return 0;
        }

        case WM_USER+NOTIFY_PAUSE: {
            playback_set_paused(true);
            update_media_controls_state();
            return 0;
        }

        case WM_USER+NOTIFY_PLAY: {
            playback_set_paused(false);
            update_media_controls_state();
            return 0;
        }

        case WM_USER+NOTIFY_PREV_TRACK: {
            ui_play_previous_track();
            return 0;
        }

        case WM_USER+NOTIFY_NEXT_TRACK: {
            ui_play_next_track();
            return 0;
        }

        case WM_USER+NOTIFY_NEW_TRACK_PLAYING: {
            update_media_controls_metadata(ui_get_playing_track());
            update_media_controls_state();
            return 0;
        }
	}
	
	return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static void create_render_target() {
	ID3D10Texture2D *texture;
	d3d.swapchain->GetBuffer(0, IID_PPV_ARGS(&texture));
	d3d.device->CreateRenderTargetView(texture, NULL, &d3d.render_target);
	texture->Release();
}

static void destroy_render_target() {
	if (d3d.render_target) {
		d3d.render_target->Release();
		d3d.render_target = NULL;
	}
}

static void resize_swapchain() {
    destroy_render_target();
    d3d.swapchain->ResizeBuffers(1, g_window.resize_width, g_window.resize_height, DXGI_FORMAT_UNKNOWN, 0);
    g_window.resize_width = g_window.resize_height = 0;
    create_render_target();
}

static bool create_d3d_device() {
    DXGI_SWAP_CHAIN_DESC swapchain = {};
	swapchain.BufferCount = 2;
	swapchain.BufferDesc.Width = 0;
	swapchain.BufferDesc.Height = 0;
	swapchain.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapchain.BufferDesc.RefreshRate.Numerator = 144;
	swapchain.BufferDesc.RefreshRate.Denominator = 1;
	swapchain.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	swapchain.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapchain.OutputWindow = g_hwnd;
	swapchain.SampleDesc.Count = 1;
	swapchain.SampleDesc.Quality = 0;
	swapchain.Windowed = TRUE;
	swapchain.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    
	int flags = 0;
#ifndef NDEBUG
	flags |= D3D10_CREATE_DEVICE_DEBUG;
#endif
    
	HRESULT result = D3D10CreateDeviceAndSwapChain(NULL, D3D10_DRIVER_TYPE_HARDWARE, NULL,
                                                   flags, D3D10_SDK_VERSION,
                                                   &swapchain, &d3d.swapchain, &d3d.device);
    
	if (result == DXGI_ERROR_UNSUPPORTED) {
		result = D3D10CreateDeviceAndSwapChain(NULL, D3D10_DRIVER_TYPE_HARDWARE, NULL,
                                               flags, D3D10_SDK_VERSION,
                                               &swapchain, &d3d.swapchain, &d3d.device);
	}
    
	if (result != S_OK) {
		show_message_box(MESSAGE_BOX_TYPE_ERROR, "Device does not support DirectX10");
		return false;
	}
    
	create_render_target();
    
	return true;
}

static void render_frame() {
    // Render background
    if (g_background.texture) {
        ImDrawList *drawlist = ImGui::GetBackgroundDrawList(ImGui::GetMainViewport());
        
        int width = g_background.width;
        int height = g_background.height;
        int winwidth = g_window.width;
        int winheight = g_window.height;
        
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
    
    // Finish ImGui frame
	ImGui::Render();
    
    // Clear buffer
	const float clear_color[4] = {0.f, 0.f, 0.f, 1.f};
	d3d.device->OMSetRenderTargets(1, &d3d.render_target, NULL);
	d3d.device->ClearRenderTargetView(d3d.render_target, clear_color);
    
    // Render ImGui state
	ImDrawData *draw_data = ImGui::GetDrawData();
	if (draw_data) {
		ImGui_ImplDX10_RenderDrawData(draw_data);
	}
    
    // Present
	g_window.is_obscured = d3d.swapchain->Present(1, 0) == DXGI_STATUS_OCCLUDED;
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
	ImGui_ImplDX10_InvalidateDeviceObjects();
	
	io.Fonts->Clear();
	
	if (path) {
		io.Fonts->AddFontFromFileTTF(path, scaled_font_size, &cfg);
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
    
	ImGui_ImplDX10_CreateDeviceObjects();
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
                  g_prefs.icon_font_size, g_window.dpi_scale);
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

static DXGI_FORMAT image_format_to_dxgi(int format) {
    switch (format) {
        case IMAGE_FORMAT_R8G8B8A8: return DXGI_FORMAT_R8G8B8A8_UNORM;
    }
    
    ASSERT(false && "Unknown image format");
    return DXGI_FORMAT_UNKNOWN;
}

static int image_format_bytes_per_pixel(int format) {
    switch (format) {
        case IMAGE_FORMAT_R8G8B8A8: return 4;
    }
    
    ASSERT(false && "Unknown image format");
    return 0;
}

Texture *create_texture_from_image(Image *image) {
    ID3D10ShaderResourceView *view;
    ID3D10Texture2D *texture;
    
    D3D10_TEXTURE2D_DESC desc = {};
	desc.Width = image->width;
	desc.Height = image->height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = image_format_to_dxgi(image->format);
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D10_USAGE_DEFAULT;
	desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
    
    D3D10_SUBRESOURCE_DATA data = {};
    data.pSysMem = image->data;
    data.SysMemPitch = image->width * image_format_bytes_per_pixel(image->format);
    
	d3d.device->CreateTexture2D(&desc, &data, &texture);
	if (!texture) {
		return NULL;
	}
    defer(texture->Release());
    
    D3D10_SHADER_RESOURCE_VIEW_DESC sr = {};
	sr.Format = desc.Format;
	sr.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
	sr.Texture2D.MipLevels = 1;
	d3d.device->CreateShaderResourceView(texture, &sr, &view);
    
    return view;
}

void destroy_texture(Texture **texture) {
    if (*texture) ((ID3D10ShaderResourceView*)*texture)->Release();
    *texture = NULL;
}

bool get_hotkey_bind_string(int index, char *buffer, int buffer_size) {
    if (!g_hotkey_is_bound[index]) return false;
    Hotkey hotkey = g_hotkeys[index];
    UINT m = hotkey.mods;
    
    if (m & MOD_CONTROL) {
        const char *c = "Ctrl + ";
        int len = (int)strlen(c);
        strncpy0(buffer, c, buffer_size);
        buffer_size -= len;
        buffer += len;
    }
    
    if (buffer_size < 0) return true;
    
    if (m & MOD_SHIFT) {
        const char *c = "Shift + ";
        int len = (int)strlen(c);
        strncpy0(buffer, c, buffer_size);
        buffer_size -= len;
        buffer += len;
    }
    
    if (buffer_size < 0) return true;
    
    if (m & MOD_ALT) {
        const char *c = "Alt + ";
        int len = (int)strlen(c);
        strncpy0(buffer, c, buffer_size);
        buffer_size -= len;
        buffer += len;
    }
    
    if (buffer_size < 0) return true;
    
    UINT scancode = MapVirtualKeyA(hotkey.vk, MAPVK_VK_TO_VSC);
    char key_name[64];
    
    GetKeyNameTextA(scancode << 16, key_name, sizeof(key_name));
    
    strncpy0(buffer, key_name, buffer_size);
    
    return true;
}

void capture_next_input_and_bind_to_hotkey(int hotkey) {
    g_capture_next_input = true;
    g_capture_input_target = hotkey;
    g_hotkey_is_bound[hotkey] = false;
    UnregisterHotKey(g_hwnd, hotkey);
}

bool is_hotkey_being_captured(int hotkey) {
    return g_capture_next_input && g_capture_input_target == hotkey;
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
    g_prefs.save_to_file(PREFS_PATH);
}

static void create_tray_icon() {
	NOTIFYICONDATAA data = {};
	
	data.cbSize = sizeof(data);
	data.hWnd = g_hwnd;
	data.uID = 1;
	data.uFlags = NIF_TIP | NIF_MESSAGE | NIF_ICON;
	data.uCallbackMessage = WM_APP + 1;
	data.uVersion = 4;
    data.hIcon = g_icon;
	
	strcpy(data.szTip, "Music Player");
	
	Shell_NotifyIconA(NIM_ADD, &data);
    
    g_tray_popup = CreatePopupMenu();
    if (g_tray_popup) {
        AppendMenuA(g_tray_popup, MF_STRING, 1, "Exit");
    }
}

static void remove_tray_icon() {
	NOTIFYICONDATAA data = {};
	
	data.cbSize = sizeof(data);
	data.hWnd = g_hwnd;
	data.uID = 1;
	
	Shell_NotifyIconA(NIM_DELETE, &data);
    
    if (g_tray_popup) {
        DestroyMenu(g_tray_popup);
    }
}

void set_window_title_message(const char *format, ...) {
    wchar_t title[512];
    char formatted_message[512];
    va_list va;
    va_start(va, format);
    vsnprintf(formatted_message, sizeof(formatted_message), format, va);
    va_end(va);
    
    _snwprintf(title, LENGTH_OF_ARRAY(title), L"ZNO MP " APP_VERSION_STRING "  |  %hs", formatted_message);
    SetWindowTextW(g_hwnd, title);
}

//-
// Drag drop
void tell_main_we_have_a_drag_drop_payload() {
    g_have_drag_drop_payload = true;
}

void tell_main_weve_dropped_the_drag_drop_payload() {
    g_drag_drop_done = true;
}

void add_to_file_drag_drop_payload(const wchar_t *path) {
    g_drag_drop_payload.
        offsets.append(g_drag_drop_payload.string_pool.append_array(path, (u32)wcslen(path)+1));
}

void clear_file_drag_drop_payload() {
    g_have_drag_drop_payload = false;
    g_drag_drop_payload.string_pool.free();
    g_drag_drop_payload.offsets.free();
}

const File_Drag_Drop_Payload& get_file_drag_drop_payload() {
    return g_drag_drop_payload;
}

void screen_to_main_window_pos(POINT *point) {
    ScreenToClient(g_hwnd, point);
}
//-

