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
#include <fileref.h>
#include "taglib_file_name_workaround.h"

// This exactly how taglib_file_new is implemented in the C API, but it takes a wchar  string
// instead
TagLib_File *taglib_file_new_wchar_(const wchar_t *filename) {
    return reinterpret_cast<TagLib_File*>(new TagLib::FileRef(filename));
}

