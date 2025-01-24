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

#ifdef _WIN32

#define VIDEO_IMPL

#include "platform.h"
#include "main.h"
#include "preferences.h"
#include "playback.h"
#include "ui.h"
#include "media_controls.h"
#include "drag_drop.h"
#include "video.h"
#include <Windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <backends/imgui_impl_win32.h>
#include <locale.h>

#define WINDOW_CLASS_NAME L"ZNO_WINDOW_CLASS"
#define WINDOW_TITLE (L"ZNO MP " APP_VERSION_STRING)
#define HOTKEYS_PATH "Hotkeys.ini"

enum {
    HOTKEY_PREV_TRACK,
    HOTKEY_NEXT_TRACK,
    HOTKEY_TOGGLE_PLAYBACK,
    HOTKEY__LAST = HOTKEY_TOGGLE_PLAYBACK,
};

#define HOTKEY__COUNT (HOTKEY__LAST+1)

struct Hotkey {
    UINT mods;
    UINT vk;
};

struct Window {
    int resize_width;
    int resize_height;
    int width;
    int height;
    float dpi_scale;
    bool is_obscured;
};

char PLATFORM_PLAYLIST_PATH[PATH_LENGTH];
char PLATFORM_CONFIG_PATH[PATH_LENGTH];
char PLATFORM_DATA_PATH[PATH_LENGTH];

static HWND g_hwnd;
static Window g_window;
static Hotkey g_hotkeys[HOTKEY__COUNT];
static bool g_hotkey_is_bound[HOTKEY__COUNT];
static HMENU g_tray_popup;
static HICON g_icon;
static bool g_capture_next_input;
static int g_capture_input_target;
#ifdef NDEBUG
// Shared event for making sure there is only one instance of ZNO running
static HANDLE g_foreground_event;
static Thread g_foreground_listener_thread;
#endif

IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT WINAPI window_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static void load_hotkeys();
static void save_hotkeys();
static void apply_hotkeys();
static void create_tray_icon();
static void remove_tray_icon();
static bool is_key_down(int vk) {return (GetKeyState(vk) * 0x8000) != 0;}
static void init_drag_drop(HWND hWnd);

bool is_hotkey_being_captured(int hotkey);
// Returns true if the hotkey is bound. If the hotkey is not bound, the buffer is unchanged
bool get_hotkey_bind_string(int hotkey, char *buffer, int buffer_size);
const char *get_hotkey_name(int hotkey);

#ifdef NDEBUG
static int foreground_listener(void *dont_care) {
    while (1) {
        if (WaitForSingleObject(g_foreground_event, INFINITE) == WAIT_OBJECT_0) {
            notify(NOTIFY_BRING_WINDOW_TO_FOREGROUND);
        }
    }

    return 0;
}
#endif

bool platform_init() {
    HINSTANCE hinstance = GetModuleHandle(NULL);

#ifdef NDEBUG
    // Check if there is another instance of ZNO running. If there is, send it a message to
    // bring the window to the foreground
    g_foreground_event = CreateEventW(NULL, FALSE, FALSE, L"ZNO_INSTANCE");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        g_foreground_event = OpenEventW(EVENT_ALL_ACCESS, FALSE, L"ZNO_INSTANCE");
        if (g_foreground_event && g_foreground_event != INVALID_HANDLE_VALUE) {
            SetEvent(g_foreground_event);
            CloseHandle(g_foreground_event);
        }
        return 0;
    }
    g_foreground_listener_thread = thread_create(NULL, &foreground_listener);
#endif
    (void)OleInitialize(NULL);
    (void)CoInitializeEx(NULL, COINIT_MULTITHREADED);
    setlocale(LC_ALL, ".65001");

	strncpy0(PLATFORM_CONFIG_PATH, ".", PATH_LENGTH);
	strncpy0(PLATFORM_PLAYLIST_PATH, "playlists", PATH_LENGTH);
    strncpy0(PLATFORM_DATA_PATH, "data", PATH_LENGTH);

    g_icon = LoadIconA(hinstance, "WindowIcon");

    // Register window class
    {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_OWNDC;
        wc.lpfnWndProc = &window_proc;
        wc.lpszClassName = WINDOW_CLASS_NAME;
        wc.hInstance = hinstance;
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
        hinstance,
        NULL);

    // Set dark title bar
    {
        BOOL on = TRUE;
        DwmSetWindowAttribute(g_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &on, sizeof(on));
    }

    UpdateWindow(g_hwnd);

    create_tray_icon();
    init_drag_drop(g_hwnd);

    install_media_controls_handler();
    video_init(g_hwnd);

	return true;
}

void platform_deinit() {
#ifdef NDEBUG
    CloseHandle(g_foreground_event);
    thread_destroy(g_foreground_listener_thread);
#endif
    video_deinit();
}

void platform_show_window(bool show) {
    if (show)
        ShowWindow(g_hwnd, SW_NORMAL);
    else
        ShowWindow(g_hwnd, SW_HIDE);
}

void platform_resize_window(int width, int height) {
    SetWindowPos(g_hwnd, 0, 0, 0, width, height, 0);
}

void platform_init_imgui() {
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplWin32_EnableDpiAwareness();
    video_init_imgui(g_hwnd);
}

float platform_get_dpi_scale() {
    return ImGui_ImplWin32_GetDpiScaleForHwnd(g_hwnd);
}

void platform_get_window_size(int *width, int *height) {
    *width = g_window.width;
    *height = g_window.height;
}

void platform_set_window_title(const char *title) {
    wchar_t title_u16[512];
    utf8_to_wchar(title, title_u16, 512);
    SetWindowTextW(g_hwnd, title_u16);
}

bool platform_poll_events() {
    MSG msg;
    bool running = true;

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
        return running;
    }

    if (!running) return false;

    // Window is minimized
    if (g_window.is_obscured) Sleep(20);

    // Resize window
    if (g_window.resize_width != 0) {
        video_resize_window(g_window.resize_width, g_window.resize_height);
        g_window.resize_width = g_window.resize_height = 0;
    }

    return running;
}

void platform_notify(int message) {
    PostMessageW(g_hwnd, message+WM_USER, 0, 0);
}

void platform_apply_preferences() {
    load_hotkeys();
    apply_hotkeys();
}

void platform_windows_show_hotkey_editor() {
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

static LRESULT WINAPI window_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    const auto& prefs = get_preferences();

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
        switch (prefs.close_policy) {
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
        apply_preferences();
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

    case WM_USER+NOTIFY_REQUEST_PAUSE: {
        playback_set_paused(true);
        update_media_controls_state();
        return 0;
    }

    case WM_USER+NOTIFY_REQUEST_PLAY: {
        playback_set_paused(false);
        update_media_controls_state();
        return 0;
    }

    case WM_USER+NOTIFY_REQUEST_PREV_TRACK: {
        ui_play_previous_track();
        return 0;
    }

    case WM_USER+NOTIFY_REQUEST_NEXT_TRACK: {
        ui_play_next_track();
        return 0;
    }

    case WM_USER+NOTIFY_PLAYBACK_STATE_CHANGE: {
        update_media_controls_state();
        return 0;
    }

    case WM_USER+NOTIFY_NEW_TRACK_PLAYING: {
        update_media_controls_metadata(ui_get_playing_track());
        update_media_controls_state();
        return 0;
    }

    case WM_USER+NOTIFY_BRING_WINDOW_TO_FOREGROUND: {
        ShowWindow(hWnd, SW_SHOW);
        SetForegroundWindow(hWnd);
        return 0;
    }
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
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

    strcpy(data.szTip, "ZNO MP");

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

void screen_to_main_window_pos(POINT *point) {
    ScreenToClient(g_hwnd, point);
}

#ifdef _WIN32
struct Drop_Target : IDropTarget {
    STGMEDIUM medium;

    HRESULT Drop(IDataObject *data, DWORD key_state, POINTL point, DWORD *effect) override {
        FORMATETC format;
        HDROP drop;

        format.cfFormat = CF_HDROP;
        format.dwAspect = DVASPECT_CONTENT;
        format.lindex = -1;
        format.ptd = NULL;
        format.tymed = TYMED_HGLOBAL;

        if (!SUCCEEDED(data->GetData(&format, &medium))) return E_UNEXPECTED;

        drop = (HDROP)medium.hGlobal;

        u32 file_count = DragQueryFile(drop, UINT32_MAX, NULL, 0);
        u32 tracks_added_count = 0;

        for (u32 i = 0; i < file_count; ++i) {
            wchar_t path[PATH_LENGTH];
            char path_u8[PATH_LENGTH];
            DragQueryFileW(drop, i, path, PATH_LENGTH);
            wchar_to_utf8(path, path_u8, PATH_LENGTH);
            add_to_file_drag_drop_payload(path_u8);
        }


        // Tell ImGui we released left mouse because Windows eats the event when dropping
        // files into the window
        ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, false);

        tell_main_weve_dropped_the_drag_drop_payload();

        return 0;
    }

    HRESULT DragEnter(IDataObject *data, DWORD key_state, POINTL point, DWORD *effect) override {
        if (*effect & DROPEFFECT_LINK) {
            ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, true);
            tell_main_we_have_a_drag_drop_payload();
            *effect &= DROPEFFECT_COPY;
            return S_OK;
        }
        log_error("Unexpected drop effect on DragEnter(): 0x%x\n", (u32)*effect);
        return E_UNEXPECTED;
    }

    HRESULT DragLeave() override {
        ReleaseStgMedium(&medium);
        clear_file_drag_drop_payload();
        return S_OK;
    }

    HRESULT DragOver(DWORD key_state, POINTL point_l, DWORD *effect) override {
        POINT point;
        point.x = point_l.x;
        point.y = point_l.y;
        screen_to_main_window_pos(&point);
        ImGui::GetIO().AddMousePosEvent((float)point.x, (float)point.y);
        //*effect = DROPEFFECT_LINK;
        *effect &= DROPEFFECT_COPY;
        return S_OK;
    }

    virtual HRESULT __stdcall QueryInterface(REFIID riid, void **ppvObject) override {
        if (riid == IID_IDropTarget) {
            *ppvObject = this;
            return S_OK;
        }
        *ppvObject = NULL;
        return E_NOINTERFACE;
    }

    virtual ULONG __stdcall AddRef(void) override {
        return 1;
    }

    virtual ULONG __stdcall Release(void) override {
        return 0;
    }

};

static void init_drag_drop(HWND hWnd) {
    static Drop_Target g_drag_drop_target;

    HRESULT result = RegisterDragDrop((HWND)hWnd, &g_drag_drop_target);

    if (!SUCCEEDED(result)) {
        log_error("RegisterDragDrop failed with code %d (0x%x)\n", (u32)result, (u32)result);
    }
}
#endif

#endif
