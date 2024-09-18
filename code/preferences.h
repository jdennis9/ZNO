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
#ifndef PREFERENCES_H
#define PREFERENCES_H

#include "defines.h"
#include <ini.h>

// Serialized
enum {
    CLOSE_POLICY_ALWAYS_ASK = 0,
    CLOSE_POLICY_MINIMIZE_TO_TRAY = 1,
    CLOSE_POLICY_EXIT = 2,
    CLOSE_POLICY__COUNT,
};

static const char *close_policy_to_string(int p) {
    switch (p) {
        case CLOSE_POLICY_ALWAYS_ASK: return "Always ask";
        case CLOSE_POLICY_MINIMIZE_TO_TRAY: return "Minimize to tray";
        case CLOSE_POLICY_EXIT: return "Exit";
    }
    return NULL;
}

struct Preferences {
    char background[PATH_LENGTH];
    char font[PATH_LENGTH];
    char theme[PATH_LENGTH];
    int font_size;
    int icon_font_size;
    int close_policy;
    
    static constexpr int FONT_SIZE_MIN = 8;
    static constexpr int FONT_SIZE_MAX = 24;
    
    void set_defaults() {
        strcpy(font, "C:\\Windows\\Fonts\\seguisb.ttf");
        font_size = 16;
        icon_font_size = 12;
    }
    
    void save_to_file(const char *path) {
        FILE *f = fopen(path, "w");
        if (!f) return;
        
        fprintf(f, "[Main]\n");
        if (background[0]) fprintf(f, "sBackgroundPath = %s\n", background);
        if (font[0]) fprintf(f, "sFontPath = %s\n", font);
        if (theme[0]) fprintf(f, "sThemePath = %s\n", theme);
        fprintf(f, "iFontSize = %d\n", font_size);
        fprintf(f, "iIconFontSize = %d\n", icon_font_size);
        fprintf(f, "iClosePolicy = %d\n", close_policy);
        
        fclose(f);
    }
    
    void load_from_file(const char *path) {
        auto callback = 
        [](void *data, const char *section, const char *key, const char *value) -> int {
            Preferences *p = (Preferences*)data;
            if (!strcmp(key, "sBackgroundPath"))
                strncpy0(p->background, value, PATH_LENGTH);
            else if (!strcmp(key, "sFontPath"))
                strncpy0(p->font, value, PATH_LENGTH);
            else if (!strcmp(key, "sThemePath"))
                strncpy0(p->theme, value, PATH_LENGTH);
            else if (!strcmp(key, "iFontSize"))
                p->font_size = clamp(atoi(value), FONT_SIZE_MIN, FONT_SIZE_MAX);
            else if (!strcmp(key, "iIconFontSize"))
                p->icon_font_size = clamp(atoi(value), FONT_SIZE_MIN, FONT_SIZE_MAX);
            else if (!strcmp(key, "iClosePolicy"))
                p->close_policy = clamp(atoi(value), (int)CLOSE_POLICY_ALWAYS_ASK, (int)CLOSE_POLICY_EXIT);
            return 1;
        };
        
        ini_parse(path, callback, this);
    }
};

#endif //PREFERENCES_H
