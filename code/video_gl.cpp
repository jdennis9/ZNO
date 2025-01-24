#ifdef __linux__
#define VIDEO_IMPL
#include "video.h"
#include <GLFW/glfw3.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_glfw.h>

static GLFWwindow *g_window;

bool video_init(void *window) {
    g_window = (GLFWwindow*)window;
    glfwMakeContextCurrent(g_window);
    return true;
}

void video_init_imgui(void *window) {
    ASSERT(ImGui_ImplOpenGL3_Init());
}

void video_deinit() {}

void video_resize_window(int width, int height) {}

// Returns true if it is ok to render
bool video_begin_frame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    return true;
}
// Returns true if the window is visible
bool video_end_frame() {
    ImDrawData *draw_data = ImGui::GetDrawData();
    if (draw_data) {
        ImGui_ImplOpenGL3_RenderDrawData(draw_data);
    }

    glfwSwapBuffers(g_window);
    return true;
}

void video_invalidate_imgui_objects() {
    ImGui_ImplOpenGL3_DestroyDeviceObjects();
}

void video_create_imgui_objects() {
    ImGui_ImplOpenGL3_CreateDeviceObjects();
}

Texture *create_texture_from_image(Image *image) {
    GLuint texture;

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image->width, image->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image->data);
    glBindTexture(GL_TEXTURE_2D, 0);

    return (Texture*)(uintptr_t)texture;
}

void destroy_texture(Texture **texture) {
    GLuint handle = (GLuint)*(uintptr_t*)texture;
    glDeleteTextures(1, &handle);
}

#endif
