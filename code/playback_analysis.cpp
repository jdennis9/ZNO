#include <math.h>
#include "playback_analysis.h"
#include "playback.h"

struct Playback_Metrics {
    f32 peak;
    bool need_update_peak;
};

static Playback_Buffer g_buffer;
static Playback_Metrics g_metrics;

f32 get_playback_peak() {
    g_metrics.need_update_peak = true;
    return g_metrics.peak;
}

f32 calc_frame_peak(u32 delta_ms) {
    Playback_Buffer_View view = {};
    f32 peak = 0.f;
    u32 frames_wanted = (g_buffer.sample_rate/1000)*delta_ms;
    get_playback_buffer_view(&g_buffer, frames_wanted, &view);
    
    const f32 *buffer = view.data[0];
    
    for (i32 i = 0; i < view.frame_count; ++i) {
        f32 v = fabsf(buffer[i]);
        peak = MAX(v, peak);
    }
    return peak;
}

void update_playback_analyzers(f32 delta_ms) {
    u32 rounded_delta = ceilf(delta_ms);
    update_playback_buffer(&g_buffer);
    
    if (g_metrics.need_update_peak) {
        g_metrics.need_update_peak = false;
        g_metrics.peak = lerp(g_metrics.peak, calc_frame_peak(rounded_delta), delta_ms*0.015f);
        //g_metrics.peak = calc_frame_peak(rounded_delta);
    }
    
}


