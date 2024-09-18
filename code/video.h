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

bool load_image_from_file(const char *filename, Image *image);
bool load_image_from_memory(const void *data, u32 data_size, Image *image);
void free_image(Image *image);
Texture *create_texture_from_image(Image *image);
Texture *create_texture_from_file(const char *path);
void destroy_texture(Texture **texture);

#endif //VIDEO_H
