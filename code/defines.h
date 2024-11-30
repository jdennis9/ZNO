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
#ifndef DEFINES_H
#define DEFINES_H

#define TAGLIB_STATIC

#include <stdint.h>
#include <limits.h>
#include <float.h>
#include <stdio.h>
#include <wchar.h>
#include <assert.h>
#include <string.h>

#define APP_VERSION_STRING "0.5.0"
#define INLINE __forceinline
#define ASSERT assert
#define PATH_LENGTH 384
#define MAX_AUDIO_CHANNELS 8

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float f32;
typedef double f64;

// Logging
#ifndef NDEBUG
#define log_debug(...) printf("debug: "); printf_s(__VA_ARGS__)
#define wlog_debug(...) printf("debug: "); wprintf_s(__VA_ARGS__)
#else
#define log_debug(...)
#define wlog_debug(...)
#endif
#define log_error(...) printf("error: "); printf_s(__VA_ARGS__)
#define log_warning(...) printf("warning: "); printf_s(__VA_ARGS__)
#define log_info(...) printf("info: "); printf_s(__VA_ARGS__)
#define wlog_error(...) printf("error: "); wprintf_s(__VA_ARGS__)
#define wlog_warning(...) printf("warning: "); wprintf_s(__VA_ARGS__)
#define wlog_info(...) printf("info: "); wprintf_s(__VA_ARGS__)

// Helpers
#define ARRAY_LENGTH(array) (sizeof(array) / sizeof((array)[0]))
#define MULTIBYTE_TO_WIDE_STRING_LITERAL(str) L##str
#define hash_string(string) XXH32(string, strlen(string), 0)
#define hash_wstring(string) XXH32(string, wcslen(string)*sizeof(wchar_t), 0)
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

// Constants
#define PI 3.14159265359f

template <typename F>
struct Defer_Holder_ {
	F f;
	Defer_Holder_(F f) : f(f) {}
	~Defer_Holder_() { f(); }
};

template <typename F>
Defer_Holder_<F> create_defer_(F f) {
	return Defer_Holder_<F>(f);
}

#define DEFER_CAT1_(x, y) x##y
#define DEFER_CAT2_(x, y) DEFER_CAT1_(x, y)
#define DEFER_DECL_(x) DEFER_CAT2_(x, __COUNTER__)
#define defer(code) auto DEFER_DECL_(defer__) = create_defer_([&](){code;})

// Common functions

template<typename T>
static INLINE void swap(T& a, T& b) {
    T temp = a;
    a = b;
    b = temp;
}

template<typename T>
static INLINE i32 linear_search(T const *haystack, u32 count, T const& needle) {
    for (u32 i = 0; i < count; ++i) {
        if (haystack[i] == needle) return i;
    }
    
    return -1;
}

template<typename T>
static INLINE T max_of(T a, T b) {
    return (a > b) ? a : b;
}

template<typename T>
static INLINE T min_of(T a, T b) {
    return (a < b) ? a : b;
}

template<typename T>
static INLINE T clamp(T a, T min, T max) {
    if (a > max) return max;
    if (a < min) return min;
    return a;
}

static inline f32 lerp(f32 v0, f32 v1, f32 t) {
    return (1 - t) * v0 + t * v1;
}

template<typename T>
static INLINE void zero_array(T *array, size_t count) {
    memset(array, 0, count * sizeof(T));
}

static INLINE void strncpy0(char *dst, const char *src, int n) {
    strncpy(dst, src, n-1);
    dst[n-1] = 0;
}

static int format_time(i64 ts, char *buffer, int buffer_size) {
    i64 hours = ts / 3600;
	i64 minutes = (ts / 60) - (hours * 60);
	i64 seconds = ts - (hours * 3600) - (minutes * 60);
	return snprintf(buffer, buffer_size, "%02lld:%02lld:%02lld", hours, minutes, seconds);
}

// platform.cpp
u32 wchar_to_utf8(const wchar_t *in, char *buffer, u32 buffer_size);
u32 utf8_to_wchar(const char *in, wchar_t *buffer, u32 buffer_size);
u64 perf_time_now();
u64 perf_time_frequency();
u64 read_whole_file(const wchar_t *path, void **buffer, bool null_terminate = false);

static inline float perf_time_to_millis(u64 ticks) {
    return ((float)ticks / (float)perf_time_frequency()) * 1000.f;
}

#define START_TIMER(var, text) struct {const char *name; u64 start;} timer__##var = {text, perf_time_now()};
#define STOP_TIMER(var) \
printf("%s: %gms\n", timer__##var.name, \
((float)(perf_time_now() - timer__##var.start) / (float)perf_time_frequency()) * 1000.f);


#endif //DEFINES_H

