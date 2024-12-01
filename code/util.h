#ifndef UTIL_H
#define UTIL_H

#include "defines.h"

// We use our on tolower because Microsoft's one is extremely slow
static inline int to_lower(int c) {
    if (c >= 'A' && c <= 'Z') {
        return c + ('a' - 'A');
    }
    return c;
}

static inline void string_to_lower(const char *in, char *out, int out_size) {
    int i = 0;
    int max = out_size - 1;
    for (i = 0; (i < max) && *in; ++i) {
        out[i] = to_lower(in[i]);
    }

    out[i] = 0;
}

static inline bool string_contains_string_ignoring_case(const char *haystack, const char *needle) {
    const char *n, *h;

    for (; *haystack; ++haystack) {
        n = needle;
        h = haystack;
        while (*h && (to_lower(*h) == to_lower(*n))) {
            n++;
            h++;
            if (*n == 0) return true;
        }
    }

    return false;
}

static inline bool string_equal_ignoring_case(const wchar_t *a, const wchar_t *b) {
    for (; *a && *b; ++a, ++b) {
        if (to_lower(*a) != to_lower(*b)) return false;
    }
    return !*a && !*b;
}

#endif
