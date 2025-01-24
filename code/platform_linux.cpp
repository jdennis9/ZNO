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
#ifdef __linux__
#define VIDEO_IMPL
#include "platform.h"
#include "os.h"
#include "video.h"
#include "main.h"
#include "ui.h"
#include "playback.h"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>

char PLATFORM_METADATA_PATH[PATH_LENGTH];
char PLATFORM_PLAYLIST_PATH[PATH_LENGTH];
char PLATFORM_CONFIG_PATH[PATH_LENGTH];
char PLATFORM_DATA_PATH[PATH_LENGTH];
char PLATFORM_IMGUI_INI_PATH[PATH_LENGTH];

static GLFWwindow *g_window;
static bool g_signals[NOTIFY__COUNT];

static void quit_callback(GLFWwindow *window) {
    g_signals[NOTIFY_QUIT] = true;
}

static void size_callback(GLFWwindow *window, int width, int height) {
    video_resize_window(width, height);
}

bool platform_init() {
#ifndef NDEBUG
    strncpy0(PLATFORM_PLAYLIST_PATH, "../playlists", PATH_LENGTH);
    strncpy0(PLATFORM_CONFIG_PATH, "..", PATH_LENGTH);
    strncpy0(PLATFORM_DATA_PATH, "../data", PATH_LENGTH);
    strncpy0(PLATFORM_METADATA_PATH, "../metadata.dat", PATH_LENGTH);
    strncpy0(PLATFORM_IMGUI_INI_PATH, "../imgui.ini", PATH_LENGTH);
#else
#error Unimplemented
#endif

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);

    g_window = glfwCreateWindow(800, 800, "ZNO MP", NULL, NULL);
    glfwHideWindow(g_window);

    video_init(g_window);

    glfwSetWindowSizeCallback(g_window, &size_callback);
    glfwSetWindowCloseCallback(g_window, &quit_callback);



    return true;
}

void platform_deinit() {
    glfwDestroyWindow(g_window);
}

void platform_init_imgui() {
    ImGui_ImplGlfw_InitForOpenGL(g_window, true);
    video_init_imgui(g_window);

    ImGuiIO& io = ImGui::GetIO();
#ifndef NDEBUG
    io.IniFilename = "../imgui.ini";
#else
#error Unimplemented
#endif
}

void platform_show_window(bool show) {
    if (show) glfwShowWindow(g_window);
    else glfwHideWindow(g_window);
}

void platform_resize_window(int width, int height) {
    glfwSetWindowSize(g_window, width, height);
}

void platform_get_window_size(int *width, int *height) {
    glfwGetWindowSize(g_window, width, height);
}

void platform_set_window_title(const char *title) {
    glfwSetWindowTitle(g_window, title);
}

float platform_get_dpi_scale() {
    return 1.f;
}

static void handle_notify_signal(int sig) {
    switch (sig) {
        case NOTIFY_REQUEST_NEXT_TRACK:
        ui_play_next_track();
        break;
        case NOTIFY_REQUEST_PREV_TRACK:
        ui_play_previous_track();
        break;
        case NOTIFY_REQUEST_PAUSE:
        playback_set_paused(true);
        break;
        case NOTIFY_REQUEST_PLAY:
        playback_set_paused(false);
        break;
        case NOTIFY_MINIMIZE_TO_TRAY:
        glfwIconifyWindow(g_window);
        break;
        case NOTIFY_BRING_WINDOW_TO_FOREGROUND:
        glfwMaximizeWindow(g_window);
        break;
    }
}

// Returns false when a quit is requested
bool platform_poll_events() {
    glfwPollEvents();

    for (int i = 0; i < NOTIFY__COUNT; ++i) {
        if ((i != NOTIFY_QUIT) && g_signals[i]) {
            handle_notify_signal(i);
            g_signals[i] = false;
        }
    }

    return g_signals[NOTIFY_QUIT] == false;
}

void platform_notify(int message) {
    g_signals[message] = true;
}

void platform_apply_preferences() {

}

#endif
