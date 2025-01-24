#ifdef __linux__
#include "audio.h"
#include <portaudio.h>

static bool g_initialized;

struct Portaudio_Data {
    PaStream *stream;
    Fill_Audio_Buffer_Callback *callback;
    void *callback_data;
    float volume;
};

static int stream_callback(const void *input, void *output, unsigned long frames, const PaStreamCallbackTimeInfo *time_info, PaStreamCallbackFlags status_flags, void *data) {
    Portaudio_Data *stream = (Portaudio_Data*)data;
    Audio_Buffer_Spec spec;
    spec.channel_count = 2;
    spec.frame_count = frames;
    spec.sample_rate = 44100;

    stream->callback(stream->callback_data, (f32*)output, &spec);

    for (unsigned long i = 0; i < frames; ++i) {
        ((f32*)output)[i] *= stream->volume;
    }

    return 0;
}

static void portaudio_interrupt(void *data) {
    //Portaudio_Data *pa = (Portaudio_Data*)data;
    //Pa_AbortStream(pa->stream);
    //Pa_StartStream(pa->stream);
}

static void portaudio_set_volume(void *data, float volume) {
    Portaudio_Data *pa = (Portaudio_Data*)data;
    pa->volume = volume;
}

static float portaudio_get_volume(void *data) {
    Portaudio_Data *pa = (Portaudio_Data*)data;
    return pa->volume;
}

static void portaudio_close(void *data) {
    Portaudio_Data *pa = (Portaudio_Data*)data;
    Pa_CloseStream(pa->stream);
}

bool open_portaudio_audio_stream(Fill_Audio_Buffer_Callback *callback, void *callback_data, Audio_Stream *stream) {
    PaError err;
    Portaudio_Data *data = new Portaudio_Data;

    stream->data = data;
    stream->set_volume_fn = &portaudio_set_volume;
    stream->get_volume_fn = &portaudio_get_volume;
    stream->close_fn = &portaudio_close;
    stream->interrupt_fn = &portaudio_interrupt;
    data->callback = callback;
    data->callback_data = callback_data;
    data->volume = 1.f;

    if (!g_initialized) {
        err = Pa_Initialize();
        if (err != paNoError) return false;
    }

    err = Pa_OpenDefaultStream(
        &data->stream,
        0,
        2,
        paFloat32,
        44100,
        256,
        &stream_callback,
        data
    );

    assert(err == paNoError);
    if (err != paNoError) return false;

    Pa_StartStream(data->stream);

    return true;
}
#endif
