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
#ifdef _WIN32
#include "os.h"
#include <windows.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <wchar.h>
#include <ini.h>

u32 wchar_to_utf8(const wchar_t *in, char *buffer, u32 buffer_size) {
    i32 ret = WideCharToMultiByte(CP_UTF8, 0, in, -1, buffer, buffer_size, NULL, NULL) - 1;
    if (ret == -1) return 0;
    return (u32)ret;
}

u32 utf8_to_wchar(const char *in, wchar_t *buffer, u32 buffer_size) {
    int ret = MultiByteToWideChar(CP_UTF8, 0, in, -1, buffer, buffer_size) - 1;
    if (ret == -1) return 0;
    return (u32)ret;
}

wchar_t *lazy_convert_path(const char *str) {
    static thread_local wchar_t buffer[PATH_LENGTH];
    utf8_to_wchar(str, buffer, PATH_LENGTH);
    return buffer;
}

Mutex create_mutex() {
    return CreateMutexA(NULL, FALSE, NULL);
}

void lock_mutex(Mutex mtx) {
    ASSERT(mtx);
    WaitForSingleObject(mtx, INFINITE);
}

void unlock_mutex(Mutex mtx) {
    ASSERT(mtx);
    ReleaseMutex(mtx);
}

void destroy_mutex(Mutex mutex) {
    if (mutex) CloseHandle(mutex);
}

struct Thread_Func_Wrapper_Data {
    void *data;
    Thread_Func *func;
    HANDLE semaphore;
};

static DWORD thread_func_wrapper(LPVOID user_data) {
    Thread_Func_Wrapper_Data *data = (Thread_Func_Wrapper_Data*)user_data;
    ReleaseSemaphore(data->semaphore, 1, NULL);
    int result = data->func(data->data);
    delete data;
    return result;
}

Thread thread_create(void *user_data, Thread_Func *func) {
    Thread_Func_Wrapper_Data *data = new Thread_Func_Wrapper_Data;
    data->data = user_data;
    data->func = func;
    data->semaphore = CreateSemaphoreW(NULL, 0, 1, NULL);

    return CreateThread(NULL, 0, &thread_func_wrapper, data, 0, 0);
}

void thread_join(Thread thread) {
    WaitForSingleObject(thread, INFINITE);
}

void thread_destroy(Thread thread) {
    CloseHandle(thread);
}

void show_message_box(Message_Box_Type type, const char *format, ...) {
    char message[4096];
    va_list va;
    va_start(va, format);
    vsnprintf(message, sizeof(message), format, va);
    va_end(va);
    
    UINT flags = 0;
    const char *caption = "Message";
    
    switch (type) {
        case MESSAGE_BOX_TYPE_ERROR:
        flags = MB_ICONERROR;
        caption = "Error";
        break;
        case MESSAGE_BOX_TYPE_WARNING:
        flags = MB_ICONWARNING;
        caption = "Warning";
        break;
        case MESSAGE_BOX_TYPE_INFO:
        flags = MB_ICONINFORMATION;
        caption = "Information";
        break;
    }
    
    MessageBoxA(NULL, message, caption, flags);
}

bool show_yes_no_dialog(const char *title, const char *format, ...) {
    char message[4096];
    va_list va;
    va_start(va, format);
    vsnprintf(message, sizeof(message), format, va);
    va_end(va);
    
    return MessageBoxA(NULL, message, title, MB_YESNO|MB_ICONWARNING) == IDYES;
}

bool show_confirm_dialog(const char *title, const char *format, ...) {
    char message[4096];
    va_list va;
    va_start(va, format);
    vsnprintf(message, sizeof(message), format, va);
    va_end(va);
    
    return MessageBoxA(NULL, message, title, MB_OKCANCEL|MB_ICONWARNING) == IDOK;
}

bool does_file_exist(const char *path) {
    struct _stat st;
    bool exists = _wstat(lazy_convert_path(path), &st) == 0;
    //wlog_debug(L"does_file_exist(%s) = %u\n", path, exists);
    return exists;
}

static COMDLG_FILTERSPEC INI_FILE_TYPES[] = {
    {L"INI Configuration File", L"*.ini"},
};

static COMDLG_FILTERSPEC AUDIO_FILE_TYPES[] = {
    {L"Supported audio file type", L"*.mp3;*.flac;*.aiff;*.ogg;*.opus;*.wav;*.ape"},
};

static COMDLG_FILTERSPEC IMAGE_FILE_TYPES[] = {
    {L"Image file", L"*.tga;*.jpeg;*.jpg;*.png"},
};

static COMDLG_FILTERSPEC FONT_FILE_TYPES[] = {
    {L"Font file", L"*.ttf;*.otf;*.ttc"},
};

static void set_filter_spec(IFileDialog *d, File_Type type) {
    switch (type) {
        case FILE_TYPE_INI: {
            d->SetFileTypes(ARRAY_LENGTH(INI_FILE_TYPES), INI_FILE_TYPES);
            break;
        }
        case FILE_TYPE_AUDIO: {
            d->SetFileTypes(ARRAY_LENGTH(AUDIO_FILE_TYPES), AUDIO_FILE_TYPES);
            break;
        }
        case FILE_TYPE_IMAGE: {
            d->SetFileTypes(ARRAY_LENGTH(IMAGE_FILE_TYPES), IMAGE_FILE_TYPES);
            break;
        }
        case FILE_TYPE_FONT: {
            d->SetFileTypes(ARRAY_LENGTH(FONT_FILE_TYPES), FONT_FILE_TYPES);
            break;
        }
    }
}

static void set_default_extension(IFileDialog *d, File_Type type) {
    switch (type) {
        case FILE_TYPE_INI: {
            d->SetDefaultExtension(L"ini");
            break;
        }
    }
}

bool open_file_save_dialog(File_Type file_type, char *buffer, int buffer_size) {
    IFileSaveDialog *dialog = NULL;
    HRESULT error = CoCreateInstance(CLSID_FileSaveDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
    if (error) {
        show_message_box(MESSAGE_BOX_TYPE_ERROR, "Failed to create IFileDialog with code %d\n", error);
        return false;
    }
    
    defer(dialog->Release());
    set_filter_spec((IFileDialog*)dialog, file_type);
    set_default_extension((IFileDialog*)dialog, file_type);
    
    error = dialog->Show(NULL);
    if (error) {
        return false;
    }
    
    IShellItem *result = NULL;
    error = dialog->GetResult(&result);
    if (error) {
        return false;
    }
    defer(result->Release());
    
    LPWSTR result_path = NULL;
    result->GetDisplayName(SIGDN_FILESYSPATH, &result_path);
    if (result_path) {
        wchar_to_utf8(result_path, buffer, buffer_size);
        CoTaskMemFree(result_path);
        return true;
    }
    
    return false;
}

static bool open_file_or_folder_select_dialog(File_Type type, char *buffer, int buffer_size, bool select_folders) {
    IFileDialog *dialog = NULL;
    HRESULT error = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
    
    if (error) {
        show_message_box(MESSAGE_BOX_TYPE_ERROR, "Failed to create IFileDialog with code %d\n", error);
        return false;
    }
    
    defer(dialog->Release());
    
    if (select_folders) dialog->SetOptions(FOS_PICKFOLDERS | FOS_PATHMUSTEXIST);
    else {
        dialog->SetOptions(FOS_FILEMUSTEXIST);
        set_filter_spec(dialog, type);
    }
    
    error = dialog->Show(NULL);
    if (error) {
        return false;
    }
    
    IShellItem *result = NULL;
    error = dialog->GetResult(&result);
    if (error) {
        return false;
    }
    defer(result->Release());
    
    LPWSTR result_path = NULL;
    result->GetDisplayName(SIGDN_FILESYSPATH, &result_path);
    if (result_path) {
        wchar_to_utf8(result_path, buffer, buffer_size);
        CoTaskMemFree(result_path);
        return true;
    }
    
    return false;
}

bool open_file_select_dialog(File_Type type, char *buffer, int buffer_size) {
    return open_file_or_folder_select_dialog(type, buffer, buffer_size, false);
}

bool open_folder_select_dialog(File_Type type, char *buffer, int buffer_size) {
    return open_file_or_folder_select_dialog(type, buffer, buffer_size, true);
}

bool open_file_multiselect_dialog(File_Type file_type, File_Iterator_Fn *iterator, void *iterator_data) {
    IFileOpenDialog *dialog = NULL;
    HRESULT error = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
    
    if (error) {
        show_message_box(MESSAGE_BOX_TYPE_ERROR, "Failed to create IFileOpenDialog with code %d\n", error);
        return false;
    }
    
    defer(dialog->Release());
    
    dialog->SetOptions(FOS_FILEMUSTEXIST | FOS_ALLOWMULTISELECT);
    set_filter_spec(dialog, file_type);
    
    error = dialog->Show(NULL);
    if (error) {
        return false;
    }
    
    IShellItemArray *results = NULL;
    error = dialog->GetResults(&results);
    if (error) {
        return false;
    }
    defer(results->Release());
    
    DWORD count = 0;
    results->GetCount(&count);
    for (u32 i = 0; i < count; ++i) {
        char path_u8[PATH_LENGTH];
        LPWSTR path = NULL;
        IShellItem *item = NULL;

        results->GetItemAt(i, &item);
        if (!item) continue;
        
        defer(item->Release());
        item->GetDisplayName(SIGDN_FILESYSPATH, &path);
        
        if (!path) continue;
        defer(CoTaskMemFree(path));

        wchar_to_utf8(path, path_u8, PATH_LENGTH);
        if (iterator(iterator_data, path_u8, false) == RECURSE_STOP) return true;
    }
    
    return true;
}

bool open_folder_multiselect_dialog(File_Type type, File_Iterator_Fn *iterator, void *iterator_data) {
    IFileOpenDialog *dialog = NULL;
    HRESULT error = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
    
    if (error) {
        show_message_box(MESSAGE_BOX_TYPE_ERROR, "Failed to create IFileOpenDialog with code %d\n", error);
        return false;
    }
    
    defer(dialog->Release());
    
    dialog->SetOptions(FOS_PATHMUSTEXIST | FOS_PICKFOLDERS | FOS_ALLOWMULTISELECT);
    
    error = dialog->Show(NULL);
    if (error) {
        return false;
    }
    
    IShellItemArray *results = NULL;
    error = dialog->GetResults(&results);
    if (error) {
        return false;
    }
    defer(results->Release());
    
    DWORD count = 0;
    results->GetCount(&count);
    for (u32 i = 0; i < count; ++i) {
        char path_u8[PATH_LENGTH];
        LPWSTR path = NULL;
        IShellItem *item = NULL;
        results->GetItemAt(i, &item);
        if (!item) continue;
        
        defer(item->Release());
        item->GetDisplayName(SIGDN_FILESYSPATH, &path);
        
        if (!path) continue;
        defer(CoTaskMemFree(path));
        
        wchar_to_utf8(path, path_u8, PATH_LENGTH);
        if (iterator(iterator_data, path_u8, true) == RECURSE_STOP) return true;
    }
    
    return true;
}

bool for_each_file_in_folder(const char *path, File_Iterator_Fn *iterator, void *iterator_data) {
    wchar_t path_buffer[PATH_LENGTH] = {};
    _snwprintf(path_buffer, PATH_LENGTH-1, L"%s\\*", lazy_convert_path(path));
    
    WIN32_FIND_DATAW find_data;
    HANDLE find_handle;
    
    find_handle = FindFirstFileW(path_buffer, &find_data);
    if (find_handle == INVALID_HANDLE_VALUE) return false;
    
    defer(FindClose(find_handle));
    
    do {
        char path_u8[PATH_LENGTH];
        if (!wcscmp(find_data.cFileName, L".") || !wcscmp(find_data.cFileName, L"..")) continue;

        _snwprintf(path_buffer, PATH_LENGTH, L"%s\\%s", lazy_convert_path(path), find_data.cFileName);
        wchar_to_utf8(path_buffer, path_u8, PATH_LENGTH);
        iterator(iterator_data, path_u8, (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0);
        memset(path_buffer, 0, sizeof(path_buffer));
    } while (FindNextFileW(find_handle, &find_data));
    
    return true;
}

bool create_directory(const char *path) {
    log_debug("create_directory(%s)\n", path);
    return CreateDirectoryW(lazy_convert_path(path), 0) == TRUE;
}

void generate_temporary_file_name(const char *base_path, char *buffer, int buffer_size) {
    int num = rand();
    while (1) {
        snprintf(buffer, buffer_size, "%s\\%x", base_path, num);
        if (!does_file_exist(buffer)) return;
        num++;
    }
}

void show_last_error_in_message_box(const char *title) {
    DWORD error_id = GetLastError();
    if (error_id == 0) return;
    
    DWORD format_flags = FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS;
    
    wchar_t message[512];
    wchar_t caption[64] = {};
    
    _snwprintf(caption, 63, L"HRESULT %x", (u32)error_id);
    
    size_t size = FormatMessageW(format_flags, NULL, error_id, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                 message, ARRAY_LENGTH(message), NULL);
    
    
    MessageBoxW(NULL, message, caption, MB_ICONERROR);
}

void delete_file(const char *path) {
    DeleteFileW(lazy_convert_path(path));
}

bool is_path_a_folder(const char *path) {
    return GetFileAttributesW(lazy_convert_path(path)) & FILE_ATTRIBUTE_DIRECTORY;
}

u64 perf_time_now() {
    LARGE_INTEGER i;
    QueryPerformanceCounter(&i);
    return i.QuadPart;
}

u64 perf_time_frequency() {
    LARGE_INTEGER i;
    QueryPerformanceFrequency(&i);
    return i.QuadPart;
}

u64 read_whole_file(const char *path, void **buffer, bool null_terminate) {
    FILE *f = _wfopen(lazy_convert_path(path), L"rb");
    if (!f) return 0;
    
    setvbuf(f, NULL, _IOFBF, 8<<10);
    fseek(f, 0, SEEK_END);
    i64 size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    *buffer = malloc(size + (null_terminate ? 1 : 0));
    fread(*buffer, size, 1, f);
    fclose(f);
    
    if (null_terminate) ((char*)*buffer)[size] = 0;
    
    return size;
}
#endif
    