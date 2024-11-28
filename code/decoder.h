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

bool decoder_open(Decoder *dec, const wchar_t *filename);
void decoder_close(Decoder *dec);
Decode_Status decoder_decode(Decoder *dec, f32 *buffer, i32 frames, i32 channels, i32 samplerate);
void decoder_seek_millis(Decoder *dec, i64 millis);
i64 decoder_get_position_millis(Decoder *dec);

#endif //DECODER_H

