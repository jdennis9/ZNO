#ifdef __linux__
#include "os.h"
#include "array.h"
#include <sys/stat.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <cwchar>

struct Thread_Func_Data {
    void *user_data;
    int (*func)(void*);
};

static void *thread_func_wrapper(void *in_data) {
    Thread_Func_Data *data = (Thread_Func_Data*)in_data;
    data->func(data->user_data);
    delete data;
    return NULL;
}

u32 utf8_to_wchar(const char *in, wchar_t *buf, u32 buf_size) {
    std::mbstate_t state = std::mbstate_t();
    return (u32) std::mbsrtowcs(buf, &in, buf_size, &state);
}

u32 wchar_to_utf8(const wchar_t *in, char *buf, u32 buf_size) {
    std::mbstate_t state = std::mbstate_t();
    return (u32) std::wcsrtombs(buf, &in, buf_size, &state);
}

Mutex create_mutex() {
    pthread_mutex_t *ret = new pthread_mutex_t;
    pthread_mutex_init(ret, NULL);
    return ret;
}

void lock_mutex(Mutex mutex) {
    if (!mutex) mutex = create_mutex();
    pthread_mutex_lock((pthread_mutex_t*)mutex);
}

void unlock_mutex(Mutex mutex) {
    pthread_mutex_unlock((pthread_mutex_t*)mutex);
}

void destroy_mutex(Mutex mutex) {
    pthread_mutex_destroy((pthread_mutex_t*)mutex);
}

Thread thread_create(void *user_data, Thread_Func *func) {
    pthread_t *thread = new pthread_t;
    Thread_Func_Data *data = new Thread_Func_Data;
    data->user_data = user_data;
    data->func = func;
    pthread_create(thread, NULL, thread_func_wrapper, data);
    return thread;
}

void thread_join(Thread thread) {
    pthread_join(*(pthread_t*)thread, NULL);
}

void thread_destroy(Thread thread) {
}

void show_message_box(Message_Box_Type type, const char *format, ...) {}
bool show_yes_no_dialog(const char *title, const char *format, ...) {return false;}
bool show_confirm_dialog(const char *title, const char *format, ...) {return false;}

bool does_file_exist(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

bool open_file_select_dialog(File_Type file_type, char *buffer, int buffer_size) {
    FILE *fp = popen("zenity --file-selection", "r");
    memset(buffer, 0, buffer_size);
    fgets(buffer, buffer_size, fp);
    return pclose(fp) == 0 && buffer[0] != 0;
}

bool open_file_save_dialog(File_Type file_type, char *buffer, int buffer_size) {return false;}
bool open_folder_select_dialog(File_Type file_type, char *buffer, int buffer_size) {return false;}

bool open_file_multiselect_dialog(File_Type file_type, File_Iterator_Fn *iterator, void *iterator_data) {
    Array<char> result = {};
    const char *file;
    FILE *fp = popen("zenity --file-selection --multiple", "r");

    while (!feof(fp)) {
        result.append(fgetc(fp));
    }

    result.append(0);

    file = strtok(result.data, "|");

    while (file) {
        iterator(iterator_data, file, is_path_a_folder(file));
        file = strtok(NULL, "|");
    }

    printf("%s\n", result.data);
    pclose(fp);

    return true;
}

bool open_folder_multiselect_dialog(File_Type file_type, File_Iterator_Fn *iterator, void *iterator_data) {return false;}
bool for_each_file_in_folder(const char *path, File_Iterator_Fn *iterator, void *iterator_data) {return false;}
bool create_directory(const char *path) {
    return mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0;
}

void generate_temporary_file_name(const char *base_path, char *buffer, int buffer_size) {
    int num = rand();
    while (1) {
        snprintf(buffer, buffer_size, "%s/%x", base_path, num);
        if (!does_file_exist(buffer)) return;
        num++;
    }
}

void show_last_error_in_message_box(const char *title) {}
void delete_file(const char *path) {
    remove(path);
}

bool is_path_a_folder(const char *path) {
    struct stat st;
    if (stat(path, &st)) return false;
    return st.st_mode & S_IFDIR;
}

u64 perf_time_now() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_nsec + (ts.tv_sec * (u64)1e9);
}

u64 perf_time_frequency() {
    return (u64)1e9;
}

u64 read_whole_file(const char *path, void **buffer, bool null_terminate) {
    FILE *f = fopen(path, "rb");
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
