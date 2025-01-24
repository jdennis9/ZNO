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
#ifndef DRAG_DROP_H
#define DRAG_DROP_H

#include "defines.h"
#include "ui.h"
#include "platform.h"
#include "main.h"
#include <imgui.h>

// This is for main.cpp
// This makes main tell ImGui that there is a drag drop payload from the shell
// next frame
void tell_main_we_have_a_drag_drop_payload();
void tell_main_weve_dropped_the_drag_drop_payload();
void clear_file_drag_drop_payload();
void add_to_file_drag_drop_payload(const char *path);

#endif //DRAG_DROP_H
