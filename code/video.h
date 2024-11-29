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

bool video_init(void *hwnd);
// Make this a separate function to make it clear that the video subsystem
// handles initializing ImGui
void video_init_imgui(void *hwnd);
void video_deinit();
// Returns true if it is ok to render
bool video_begin_frame();
// Returns true if the window is visible
bool video_end_frame();
void video_resize_window(int width, int height);

void video_invalidate_imgui_objects();
void video_create_imgui_objects();

Texture *create_texture_from_image(Image *image);
void destroy_texture(Texture **texture);

bool load_image_from_file(const char *filename, Image *image);
bool load_image_from_memory(const void *data, u32 data_size, Image *image);
void free_image(Image *image);


#endif //VIDEO_H
