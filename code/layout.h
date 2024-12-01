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
#ifndef LAYOUT_H
#define LAYOUT_H

#include "defines.h"

typedef int Layout_Select_Action;

enum {
    LAYOUT_SELECT_ACTION_LOAD,
    LAYOUT_SELECT_ACTION_DELETE,
};

void layout_init();
void layout_show_selector();
void layout_show_deleter();
const char *layout_show_custom_name_selecor();
void layout_save_current(const char *name);
void layout_overwrite_with_current(i32 index);
i32 layout_get_index_from_name(const char *name);

#endif //LAYOUT_H
