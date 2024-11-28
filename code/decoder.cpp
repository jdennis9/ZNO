#include "decoder.h"

bool decoder_open(Decoder *dec, const wchar_t *filename) {
    decoder_close(dec);
    dec->file = sf_wchar_open(filename, SFM_READ, &dec->info);
    return dec->file != nullptr;
}

void decoder_close(Decoder *dec) {
    if (dec->file) sf_close(dec->file);
    if (dec->resampler) src_delete(dec->resampler);
    *dec = Decoder{};
}

Decode_Status decoder_decode(Decoder *dec, f32 *buffer, i32 frames, i32 channels, i32 samplerate) {
    bool needs_resampling = dec->info.samplerate != samplerate;
    
    zero_array(buffer, frames * channels);
    
    if (!needs_resampling) {
        sf_count_t frames_read = sf_readf_float(dec->file, buffer, frames);
        dec->frame_index += frames_read;
        if (frames_read == 0) {
            return DECODE_STATUS_EOF;
        }
        else if (frames_read < frames) {
            return DECODE_STATUS_PARTIAL;
        }
        
        return DECODE_STATUS_COMPLETE;
    }
    else {
        if (!dec->resampler) {
            int error = 0;
            dec->resampler = src_new(SRC_SINC_FASTEST, channels, &error);
            if (!dec->resampler) return DECODE_STATUS_EOF;
        }
        
        SRC_DATA src = {};
        f32 in_to_out_sample_ratio = (f32)samplerate/(f32)dec->info.samplerate;
        i32 input_frame_count = (i32)ceilf(frames / in_to_out_sample_ratio);
        f32 *pre_resample_buffer = (f32*)malloc(input_frame_count * channels * sizeof(f32));
        defer(free(pre_resample_buffer));
        
        sf_count_t frames_read = sf_readf_float(dec->file, pre_resample_buffer, input_frame_count);
        if (frames_read == 0) return DECODE_STATUS_EOF;
        
        dec->frame_index += frames_read;
        
        src.data_in = pre_resample_buffer;
        src.data_out = buffer;
        src.input_frames = input_frame_count;
        src.output_frames = frames;
        src.src_ratio = (f64)in_to_out_sample_ratio;
        
        src_process(dec->resampler, &src);
        
        if (frames_read < input_frame_count) return DECODE_STATUS_PARTIAL;
        return DECODE_STATUS_COMPLETE;
    }
}

void decoder_seek_millis(Decoder *dec, i64 millis) {
    if (!dec->file) return;
    i64 frame = dec->info.samplerate * (millis/1000);
    dec->frame_index = sf_seek(dec->file, frame, SEEK_SET);
}

i64 decoder_get_position_millis(Decoder *dec) {
    return (i64)(dec->frame_index / dec->info.samplerate) * 1000;
}
    
    