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
#include "ui_functions.h"
#include "theme.h"
#include <imgui_internal.h>

// Trim spaces from string
static void trim_string(const char *str, char *buffer, int buffer_size) {
	while (*str && isspace(*str)) str++;
	if (!*str) return;
	int written = 0;
	
	while (*str && !isspace(*str) && written < (buffer_size-1)) {
		buffer[written] = *str;
		str++;
		written++;
	}
	
	buffer[written] = 0;
}

static void *settings_open_fn(ImGuiContext *ctx, ImGuiSettingsHandler *handler, const char *name) {
	int window = get_window_from_name(name);
	// Have to add 1 because returning NULL causes ImGui to ignore this entry
	return (void*)(uintptr_t)(window + 1);
}

static void settings_read_line_fn(ImGuiContext *ctx, ImGuiSettingsHandler *handler, 
								  void *entry, const char *line) {
    if (!entry) return;
	// -1 because we added 1 in settings_open_fn()
	int window = (int)((uintptr_t)entry - 1);
	const char *name_ptr = line;
	const char *value_ptr = strchr(line, '=');
	if (!value_ptr) return;
	value_ptr++;
	
	char name[8];
	char value[8];
	
	trim_string(name_ptr, name, sizeof(name));
	trim_string(value_ptr, value, sizeof(value));
	
	if (!strcmp(name, "Open")) {
		int open = atoi(value);
		set_window_showing(window, open);
	}
}

static void settings_write_fn(ImGuiContext *ctx, ImGuiSettingsHandler *handler, ImGuiTextBuffer *buf) {
	for (int window = 0; window < WINDOW__COUNT; ++window) {
		buf->appendf("[ZNO][%s]\n", get_window_internal_name(window));
		buf->appendf("Open = %u\n", is_window_open(window));
	}
}

void register_imgui_settings_handler() {
    ImGuiSettingsHandler handler = {};
    handler.TypeName = "ZNO";
	handler.TypeHash = ImHashStr(handler.TypeName);
	handler.ReadOpenFn = &settings_open_fn;
	handler.ReadLineFn = &settings_read_line_fn;
	handler.WriteAllFn = &settings_write_fn;
	
	ImGui::AddSettingsHandler(&handler);
}

bool circle_handle_slider(const char *str_id, float *p_value, float min, float max, ImVec2 size) {
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
	ImGuiWindow *window = ImGui::GetCurrentWindow();
	ImVec2 available_size = ImGui::GetContentRegionAvail();
	ImVec2 cursor = ImGui::GetCursorScreenPos();
	ImVec2 mouse = ImGui::GetMousePos();
	float rel_pos = (*p_value - min) / (max - min);
	ImGuiID id = ImGui::GetID(str_id);
	bool active = id == ImGui::GetActiveID();
	ImGuiStyle& style = ImGui::GetStyle();
	ImVec2 text_size = ImGui::CalcTextSize(str_id);
	
    if (size.x <= 0.f) size.x = available_size.x - style.WindowPadding.x;
    if (size.y <= 0.f) size.y = available_size.y - style.WindowPadding.y;
    
    ImVec2 clickbox_size = {
		size.x + (style.FramePadding.x * 2.f),
		size.y + (style.FramePadding.y * 2.f),
	};
    
	const float handle_radius = size.y/2.f;
	ImVec2 bg_pos = {
		cursor.x, 
		cursor.y + style.FramePadding.y + size.y/4.f,
	};
	ImVec2 handle_center = {bg_pos.x + (size.x * rel_pos), bg_pos.y + (size.y*.25f)};
	
	draw_list->AddRectFilled(bg_pos,
							 ImVec2{bg_pos.x + size.x, bg_pos.y + size.y*0.5f},
							 ImGui::GetColorU32(style.Colors[ImGuiCol_Header]), 4.f);
	draw_list->AddRectFilled(bg_pos,
							 ImVec2{bg_pos.x + (size.x * rel_pos), bg_pos.y + size.y*0.5f},
							 ImGui::GetColorU32(style.Colors[ImGuiCol_HeaderActive]), 4.f);
	
	draw_list->AddCircleFilled(handle_center,
							   handle_radius,
							   ImGui::GetColorU32(style.Colors[ImGuiCol_HeaderActive]));
	
	
	
	ImGui::PushID(id);
	
	if (ImGui::InvisibleButton(str_id, clickbox_size,
							   ImGuiButtonFlags_PressedOnClick|ImGuiButtonFlags_Repeat)) {
		active = true;
		ImGui::SetActiveID(id, window);
	}
	
	if (active &&
		(ImGui::IsMouseClicked(ImGuiMouseButton_Left)||ImGui::IsMouseDown(ImGuiMouseButton_Left))) {
		rel_pos = ImClamp((mouse.x - cursor.x) / size.x, 0.f, 1.f);
		*p_value = ImLerp(min, max, rel_pos);
	}
	
	if (active && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
		ImGui::ClearActiveID();
		active = false;
	}
	
	if (active) {
		ImGui::SetActiveID(id, window);
	}
	
	if (active || ImGui::IsItemHovered()) {
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
	}
	ImGui::PopID();
	
	return active;
}

/*static void draw_progress_rect(ImDrawList *draw_list, float progress, ImVec2 position, ImVec2 size, u32 fg, u32 bg) {
    ImVec2 min = position;
    ImVec2 mid_max = {
        min.x + size.x*progress,
        min.y + size.y,
    };
    ImVec2 max = {
        min.x + size.x,
        min.y + size.y,
    };
    
    draw_list->AddRectFilled(min, max, bg);
    draw_list->AddRectFilled(min, mid_max, fg);
}*/

void peak_meter_widget(float value, ImVec2 size) {
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
	ImVec2 available_size = ImGui::GetContentRegionAvail();
	ImVec2 cursor = ImGui::GetCursorScreenPos();
	ImGuiStyle& style = ImGui::GetStyle();
    
    if (size.y == 0.f) {
        size.y = available_size.y;
    }
    
    //value = log2f(value + 1.f);
    value = clamp(value, 0.f, 1.f);
    
    ImVec2 min = {
        cursor.x,
        cursor.y + style.FramePadding.y,
    };
    
    ImVec2 mid_max = {
        min.x + size.x*value,
        min.y + size.y,
    };
    
    ImVec2 max = {
        min.x + size.x,
        min.y + size.y,
    };
    
    u32 low_color = get_theme_color(THEME_COLOR_PEAK_METER);
    u32 high_color = get_theme_color(THEME_COLOR_PEAK_METER_BG);
    draw_list->AddRectFilled(min, max, high_color);
    draw_list->AddRectFilled(min, mid_max, low_color);
    ImGui::Dummy(size);
}

bool begin_status_bar() {
    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoScrollbar|
        ImGuiWindowFlags_NoSavedSettings|
        ImGuiWindowFlags_MenuBar;
        
    bool show = ImGui::BeginViewportSideBar("##status_bar", 
                                            ImGui::GetMainViewport(), 
                                            ImGuiDir_Down, 
                                            ImGui::GetFrameHeight(),
                                            window_flags);
    if (show) {
        return ImGui::BeginMenuBar();
    }
    return false;
}

void end_status_bar() {
    ImGui::EndMenuBar();
}

bool begin_window_drag_drop_target(const char *str_id) {
    ImGuiID id = ImGui::GetID(str_id);
    ImRect rect;
    ImVec2 pos = ImGui::GetWindowPos();
    ImVec2 size = ImGui::GetWindowSize();
    rect.Min = pos;
    rect.Max = ImVec2(pos.x + size.x, pos.y + size.y);
    
    return ImGui::BeginDragDropTargetCustom(rect, id);
}
    
    