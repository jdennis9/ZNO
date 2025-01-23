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
#ifndef OS_H
#define OS_H

#include "defines.h"

typedef void *Mutex;
typedef void *Thread;
typedef int Thread_Func(void *data);

enum Message_Box_Type {
    MESSAGE_BOX_TYPE_INFO,
    MESSAGE_BOX_TYPE_WARNING,
    MESSAGE_BOX_TYPE_ERROR,
};

enum {
    FILE_DIALOG_SELECT_FOLDER=0x1,
    FILE_DIALOG_FILE_MUST_EXIST=0x2,
};

enum {
    FILE_TYPE_AUDIO,
    FILE_TYPE_IMAGE,
    FILE_TYPE_INI,
    FILE_TYPE_FONT,
    FILE_TYPE__COUNT,
};

typedef u8 File_Type;

enum {
    FILE_DOMAIN_DEFAULT,
    FILE_DOMAIN_MUSIC,
    FILE_DOMAIN__COUNT,
};

enum {
    RECURSE_CONTINUE,
    RECURSE_STOP,
};

typedef int Recurse_Command;
typedef Recurse_Command File_Iterator_Fn(void *data, const char *path, bool is_folder);

Mutex create_mutex();
void lock_mutex(Mutex mutex);
void unlock_mutex(Mutex mutex);
void destroy_mutex(Mutex mutex);
Thread thread_create(void *user_data, Thread_Func *func);
void thread_join(Thread thread);
void thread_destroy(Thread thread);
void show_message_box(Message_Box_Type type, const char *format, ...);
bool show_yes_no_dialog(const char *title, const char *format, ...);
bool show_confirm_dialog(const char *title, const char *format, ...);
bool does_file_exist(const char *path);
bool open_file_select_dialog(File_Type file_type, char *buffer, int buffer_size);
bool open_file_save_dialog(File_Type file_type, char *buffer, int buffer_size);
bool open_folder_select_dialog(File_Type file_type, char *buffer, int buffer_size);
bool open_file_multiselect_dialog(File_Type file_type, File_Iterator_Fn *iterator, void *iterator_data);
bool open_folder_multiselect_dialog(File_Type file_type, File_Iterator_Fn *iterator, void *iterator_data);
bool for_each_file_in_folder(const char *path, File_Iterator_Fn *iterator, void *iterator_data);
bool create_directory(const char *path);
void generate_temporary_file_name(const char *base_path, char *buffer, int buffer_size);
void show_last_error_in_message_box(const char *title);
void delete_file(const char *path);
bool is_path_a_folder(const char *path);


#endif //OS_H
