#ifndef VIDEO_H
#define VIDEO_H

#include "defines.h"

enum {
    IMAGE_FORMAT_R8G8B8A8,
};

struct Image {
    u8 *data;
    i32 width, height;
    i32 format;
    u32 flags;
};

typedef void Texture;

#ifdef VIDEO_IMPL
// window is HWND on Windows and GLFWwindow* on Linux
bool video_init(void *window);
// Make this a separate function to make it clear that the video subsystem
// handles initializing ImGui
void video_init_imgui(void *window);
void video_deinit();
void video_resize_window(int width, int height);
#endif

// Returns true if it is ok to render
bool video_begin_frame();
// Returns true if the window is visible
bool video_end_frame();

void video_invalidate_imgui_objects();
void video_create_imgui_objects();

Texture *create_texture_from_image(Image *image);
void destroy_texture(Texture **texture);

bool load_image_from_file(const char *filename, Image *image);
bool load_image_from_memory(const void *data, u32 data_size, Image *image);
void free_image(Image *image);


#endif //VIDEO_H
