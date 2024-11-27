#include <imgui.h>
#include <math.h>
#include <kissfft/kiss_fftr.h>
#include "playback_analysis.h"
#include "playback.h"

#define SG_BAND_COUNT 11
#define PEAK_ROUGHNESS 0.015f
#define SPECTRUM_ROUGHNESS 0.03f

static int SG_BAND_OFFSETS[] = {
    0,
    50,
    100,
    150,
    250,
    500,
    800,
    1200,
    2000,
    4000,
    6000,
    20000,
};

struct Spectrum {
    f32 peaks[SG_BAND_COUNT];
};

struct Playback_Metrics {
    Spectrum spectrum;
    f32 peak;
    bool need_update_peak;
    bool need_update_spectrum;
};

static Playback_Buffer g_buffer;
static Playback_Metrics g_metrics;

static void hann_window(Playback_Buffer_View *in, Playback_Buffer_View *out) {
    f32 *out_data[MAX_AUDIO_CHANNELS];
    const int n = in->frame_count;

    out->frame_count = in->frame_count;
    out->channel_count = in->channel_count;

    for (int channel = 0; channel < in->channel_count; ++channel) {
        out_data[channel] = (f32 *)malloc(sizeof(f32) * in->frame_count);
        f32 *channel_data = out_data[channel];
        memcpy(channel_data, in->data[channel], n * sizeof(f32));

        for (int i = 0; i < in->frame_count; ++i) {
            f32 mul = 0.5f * (1 - cosf(2 * PI * i / (n - 1)));
            channel_data[i] *= mul;
        }

        out->data[channel] = channel_data;
    }
}

static void free_windowed_view(Playback_Buffer_View *view) {
    for (int i = 0; i < view->channel_count; ++i) {
       free((void*)view->data[i]);
    }
}

f32 get_playback_peak() {
    g_metrics.need_update_peak = true;
    return g_metrics.peak;
}

f32 calc_frame_peak(Playback_Buffer_View *view) {
    f32 peak = 0.f;
    const f32 *buffer = view->data[0];
    
    for (i32 i = 0; i < view->frame_count; ++i) {
        f32 v = fabsf(buffer[i]);
        peak = MAX(v, peak);
    }
    return peak;
}

static void calc_spectrum(Playback_Buffer_View *view, Spectrum *sg) {
    static kiss_fftr_cfg cfg = NULL;
    static u32 cfg_frame_count = 0;
    if (!cfg) {
        cfg_frame_count = view->frame_count;
        cfg = kiss_fftr_alloc(cfg_frame_count, 0, NULL, NULL);
    }

    if (view->frame_count != cfg_frame_count) {
        kiss_fftr_free(cfg);
        cfg_frame_count = view->frame_count;
        cfg = kiss_fftr_alloc(cfg_frame_count, 0, NULL, NULL);
    }

    kiss_fft_cpx *buffer = (kiss_fft_cpx*)malloc(sizeof(kiss_fft_cpx) * view->frame_count);
    defer(free(buffer));

    kiss_fftr(cfg, view->data[0], buffer);

    int output_count = (view->frame_count / 2) + 1;
    int base_band_width = 60;
    int freq_step = 22050 / view->frame_count;

    for (u32 i = 0; i < SG_BAND_COUNT; ++i) {
        sg->peaks[i] = 0.f;
    }

    for (int i = 0; i < output_count; ++i) {
        int freq = i * freq_step;
        int band = 0;

        while (!(freq >= SG_BAND_OFFSETS[band] && freq <= SG_BAND_OFFSETS[band + 1]) && band < (SG_BAND_COUNT-1))
            band++;

        kiss_fft_cpx frame = buffer[i];

        f32 mag = sqrtf((frame.r * frame.r) + (frame.i * frame.i));
        mag = log10f(mag);
        if (mag < 0.f) mag = 0.f;

        //if (band == SG_BAND_COUNT - 1 || band == 0) {
        //    log_debug("%d %g\n", freq, mag);
        //}

        sg->peaks[band] = MAX(mag, sg->peaks[band]);
    }

    for (int i = 0; i < SG_BAND_COUNT; ++i) {
        sg->peaks[i] /= 2.6f;
    }
}

void show_spectrum_widget(const char *str_id, float width) {
    const Spectrum &sg = g_metrics.spectrum;
    g_metrics.need_update_spectrum = true;
    ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
    ImGui::PlotHistogram(str_id, sg.peaks, SG_BAND_COUNT, 0, NULL, 0.f, 1.f, ImVec2(width, 0));
    ImGui::PopStyleColor();
}

void show_spectrum_ui() {
    g_metrics.need_update_spectrum = true;

    ImDrawList *drawlist = ImGui::GetWindowDrawList();
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImVec2 region = ImGui::GetContentRegionAvail();
    f32 bar_width = region.x / SG_BAND_COUNT;
    const Spectrum &sg = g_metrics.spectrum;

    static f32 max_detected = 0.f;

    /*for (u32 i = 0; i < SG_BAND_COUNT; ++i) {
        f32 s = sg.peaks[i] / 2.6f;
        ImVec2 max = {cursor.x + (bar_width * (i+1)), cursor.y + region.y};
        ImVec2 min = {max.x - bar_width + 1, cursor.y + region.y - (s * region.y)};
        drawlist->AddRectFilled(min, max, ImGui::GetColorU32(ImGuiCol_PlotHistogram));

        max_detected = MAX(sg.peaks[i], max_detected);
    }*/

    ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
    ImGui::PlotHistogram("##spectrum", sg.peaks, SG_BAND_COUNT, 0, NULL, 0.f, 1.f, region);
    ImGui::PopStyleColor();

    //ImGui::Text("Max: %g", max_detected);
}

void update_playback_analyzers(f32 delta_ms) {
    u32 rounded_delta = ceilf(delta_ms);
    update_playback_buffer(&g_buffer);
    
    Playback_Buffer_View windowed_view = {};
    Playback_Buffer_View view = {};
    f32 peak = 0.f;
    u32 frames_wanted = (g_buffer.sample_rate/1000)*rounded_delta;
    get_playback_buffer_view(&g_buffer, frames_wanted, &view);
    
    if (view.frame_count == 0 || !view.data[0]) {
        for (int i = 0; i < SG_BAND_COUNT; ++i) {
            g_metrics.spectrum.peaks[i] = lerp(g_metrics.spectrum.peaks[i], 0, delta_ms*SPECTRUM_ROUGHNESS);
        }

        g_metrics.peak = lerp(g_metrics.peak, 0, delta_ms*PEAK_ROUGHNESS);

        return;
    }
    
    hann_window(&view, &windowed_view);
    defer(free_windowed_view(&windowed_view));

    if (g_metrics.need_update_peak) {
        g_metrics.need_update_peak = false;
        g_metrics.peak = lerp(g_metrics.peak, calc_frame_peak(&view), delta_ms*PEAK_ROUGHNESS);
    }

    if (g_metrics.need_update_spectrum) {
        Spectrum frame_sg;
        f32 *peaks = g_metrics.spectrum.peaks;

        calc_spectrum(&windowed_view, &frame_sg);
        for (u32 i = 0; i < SG_BAND_COUNT; ++i) {
            peaks[i] = lerp(peaks[i], frame_sg.peaks[i], delta_ms*SPECTRUM_ROUGHNESS);
        }
        g_metrics.need_update_spectrum = false;
    }
}


