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
#include <imgui.h>
#include <math.h>
#include <kissfft/kiss_fftr.h>
#include "playback_analysis.h"
#include "playback.h"
#include "ui.h"

#define SG_BAND_COUNT 20
#define PEAK_ROUGHNESS 0.015f
#define SPECTRUM_ROUGHNESS 0.03f
#define CAPTURE_CHANNELS PLAYBACK_CAPTURE_CHANNELS

static int SG_BAND_OFFSETS[] = {
    0,
    50,
    70,
    100,
    130,
    180,
    250,
    330,
    450,
    620,
    850,
    1200,
    1600,
    2200,
    3000,
    4100,
    5600,
    7700,
    11000,
    14000,
    20000,
};

struct Spectrum {
    f32 peaks[SG_BAND_COUNT];
};

struct Playback_Metrics {
    Spectrum spectrum;
    f32 peak[MAX_AUDIO_CHANNELS];
    bool need_update_peak;
    bool need_update_spectrum;
};

static Playback_Buffer g_buffer;
static Playback_Metrics g_metrics;

static void hann_window(Playback_Buffer_View *in, Playback_Buffer_View *out) {
    f32 *out_data[MAX_AUDIO_CHANNELS];
    const int n = in->frame_count;

    out->frame_count = in->frame_count;
    out->channels = in->channels;

    for (int channel = 0; channel < in->channels; ++channel) {
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
    for (int i = 0; i < view->channels; ++i) {
       free((void*)view->data[i]);
    }
}

f32 get_playback_peak() {
    g_metrics.need_update_peak = true;
    f32 sum = 0.f;
    i32 channels = g_buffer.channels;

    for (u32 i = 0; i < channels; ++i) {
        sum += g_metrics.peak[i];
    }

    return sum / channels;
}

int get_playback_channel_peaks(f32 *out) {
    g_metrics.need_update_peak = true;
    for (int i = 0; i < g_buffer.channels; ++i) out[i] = g_metrics.peak[i];
    return g_buffer.channels;
}

void calc_frame_peak(Playback_Buffer_View *view, f32 *out) {

    for (i32 ch = 0; ch < view->channels; ++ch) {
        const f32 *buffer = view->data[ch];
        out[ch] = 0.f;
        for (i32 i = 0; i < view->frame_count; ++i) {
            f32 v = fabsf(buffer[i]);
            out[ch] = MAX(v, out[ch]);
        }
    }
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
    ImGuiTableFlags table_flags = ImGuiTableFlags_SizingStretchProp |
        ImGuiTableFlags_BordersOuterV | ImGuiTableFlags_BordersInnerV;
    
    ImDrawList *drawlist = ImGui::GetWindowDrawList();
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImVec2 region = ImGui::GetContentRegionAvail();
    f32 bar_width = region.x / SG_BAND_COUNT;
    const Spectrum &sg = g_metrics.spectrum;

    ui_push_mini_font();
    f32 line_height = ImGui::GetTextLineHeight();
    f32 max_bar_height = region.y - line_height;
    for (u32 band = 0; band < SG_BAND_COUNT; ++band) {
        f32 peak = sg.peaks[band];
        char freq_text[8] = {};
        int freq = SG_BAND_OFFSETS[band+1];
        f32 y_offset = cursor.y + region.y - line_height;

        if (freq < 1000) snprintf(freq_text, 7, "%d", freq);
        else snprintf(freq_text, 7, "%.1fK", (f32)freq / 1000.f);

        drawlist->AddText(ImVec2(cursor.x, y_offset),
            ImGui::GetColorU32(ImGuiCol_TextDisabled), freq_text);

        drawlist->AddRectFilled(
            ImVec2(cursor.x, y_offset),
            ImVec2(cursor.x + bar_width, y_offset - (peak * max_bar_height)),
            ImGui::GetColorU32(ImGuiCol_PlotHistogram));

        cursor.x += bar_width + 1;
    }
    ui_pop_mini_font();
}

void show_channel_peaks_ui() {
    g_metrics.need_update_peak = true;
    ImVec2 region = ImGui::GetContentRegionAvail();
    ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
    ImGui::PlotHistogram("##peak", g_metrics.peak, g_buffer.channels, 0, NULL, 0.f, 1.f, region);
    ImGui::PopStyleColor();
}

void update_playback_analyzers(f32 delta_ms) {
    u32 rounded_delta = (u32)ceilf(delta_ms);
    playback_update_capture_buffer(&g_buffer);
    
    Playback_Buffer_View windowed_view = {};
    Playback_Buffer_View view = {};
    u32 frames_wanted = (g_buffer.sample_rate/1000)*rounded_delta;
    get_playback_buffer_view(&g_buffer, frames_wanted, &view);
    
    if (view.frame_count == 0 || !view.data[0]) {
        for (int i = 0; i < SG_BAND_COUNT; ++i) {
            g_metrics.spectrum.peaks[i] = lerp(g_metrics.spectrum.peaks[i], 0, delta_ms*SPECTRUM_ROUGHNESS);
        }

        for (int i = 0; i < g_buffer.channels; ++i) {
            g_metrics.peak[i] = lerp(g_metrics.peak[i], 0.f, delta_ms*PEAK_ROUGHNESS);
        }

        return;
    }
    
    hann_window(&view, &windowed_view);
    defer(free_windowed_view(&windowed_view));

    if (g_metrics.need_update_peak) {
        g_metrics.need_update_peak = false;
        f32 cur_peak[MAX_AUDIO_CHANNELS];
        calc_frame_peak(&view, cur_peak);
        for (int ch = 0; ch < g_buffer.channels; ++ch) {
            g_metrics.peak[ch] = lerp(g_metrics.peak[ch], cur_peak[ch], delta_ms*PEAK_ROUGHNESS);
        }
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


