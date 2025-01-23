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
#ifndef DECODER_H
#define DECODER_H

#include "defines.h"
#include "array.h"
#include "audio.h"
#include <sndfile.h>
#include <samplerate.h>

enum Decode_Status {
    DECODE_STATUS_COMPLETE,
    DECODE_STATUS_PARTIAL,
    DECODE_STATUS_EOF,
};

struct Decoder {
    SNDFILE *file;
    SRC_STATE *resampler;
    SF_INFO info;
    i64 frame_index;
};

bool decoder_open(Decoder *dec, const char *filename);
void decoder_close(Decoder *dec);
Decode_Status decoder_decode(Decoder *dec, f32 *buffer, i32 frames, i32 channels, i32 samplerate);
int decoder_get_bitrate(Decoder *dec);
void decoder_seek_millis(Decoder *dec, i64 millis);
i64 decoder_get_position_millis(Decoder *dec);

#endif //DECODER_H

