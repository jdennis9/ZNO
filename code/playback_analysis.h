#ifndef PLAYBACK_ANALYSIS_H
#define PLAYBACK_ANALYSIS_H

#include "defines.h"

void update_playback_analyzers(f32 delta_ms);
f32 get_playback_peak();
// Show the spectrogram as an ImGui histogram
void show_spectrum_widget(const char *str_id, float width = 0.f);
// Show the spectrogram in a window, occupying the whole window
void show_spectrum_ui();

#endif //PLAYBACK_ANALYSIS_H
