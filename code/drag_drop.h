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
#include <windows.h>
#include <imgui.h>

// This is for main.cpp

// We need to convert positions from drag events from screen space
// to the main window for ImGui
void screen_to_main_window_pos(POINT *p);

// This makes main tell ImGui that there is a drag drop payload from the shell
// next frame
void tell_main_we_have_a_drag_drop_payload();
void tell_main_weve_dropped_the_drag_drop_payload();
void clear_file_drag_drop_payload();
void add_to_file_drag_drop_payload(const wchar_t *path);

struct Drop_Target : IDropTarget {
    STGMEDIUM medium;
    
    HRESULT Drop(IDataObject *data, DWORD key_state, POINTL point, DWORD *effect) override {
        FORMATETC format;
        HDROP drop;
        
        format.cfFormat = CF_HDROP;
        format.dwAspect = DVASPECT_CONTENT;
        format.lindex = -1;
        format.ptd = NULL;
        format.tymed = TYMED_HGLOBAL;
        
        if (!SUCCEEDED(data->GetData(&format, &medium))) return E_UNEXPECTED;
        
        drop = (HDROP)medium.hGlobal;
        
        u32 file_count = DragQueryFile(drop, UINT32_MAX, NULL, 0);
        u32 tracks_added_count = 0;
        
        for (u32 i = 0; i < file_count; ++i) {
            wchar_t path[PATH_LENGTH];
            DragQueryFileW(drop, i, path, PATH_LENGTH);
            add_to_file_drag_drop_payload(path);
        }
        
        
        // Tell ImGui we released left mouse because Windows eats the event when dropping
        // files into the window
        ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, false);
        
        tell_main_weve_dropped_the_drag_drop_payload();
        
        return 0;
    }
    
    HRESULT DragEnter(IDataObject *data, DWORD key_state, POINTL point, DWORD *effect) override {
        if (*effect & DROPEFFECT_LINK) {
            ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, true);
            tell_main_we_have_a_drag_drop_payload();
            *effect &= DROPEFFECT_COPY;
            return S_OK;
        }
        log_error("Unexpected drop effect on DragEnter(): 0x%x\n", (u32)*effect);
        return E_UNEXPECTED;
    }
    
    HRESULT DragLeave() override {
        ReleaseStgMedium(&medium);
        clear_file_drag_drop_payload();
        return S_OK;
    }
    
    HRESULT DragOver(DWORD key_state, POINTL point_l, DWORD *effect) override {
        POINT point;
        point.x = point_l.x;
        point.y = point_l.y;
        screen_to_main_window_pos(&point);
        ImGui::GetIO().AddMousePosEvent((float)point.x, (float)point.y);
        //*effect = DROPEFFECT_LINK;
        *effect &= DROPEFFECT_COPY;
        return S_OK;
    }
    
    virtual HRESULT __stdcall QueryInterface(REFIID riid, void **ppvObject) override {
        if (riid == IID_IDropTarget) {
            *ppvObject = this;
            return S_OK;
        }
        *ppvObject = NULL;
        return E_NOINTERFACE;
    }
    
    virtual ULONG __stdcall AddRef(void) override {
        return 1;
    }
    
    virtual ULONG __stdcall Release(void) override {
        return 0;
    }
    
};

static void init_drag_drop(HWND hWnd) {
    static Drop_Target g_drag_drop_target;
    
    HRESULT result = RegisterDragDrop((HWND)hWnd, &g_drag_drop_target);
    
    if (!SUCCEEDED(result)) {
        log_error("RegisterDragDrop failed with code %d (0x%x)\n", (u32)result, (u32)result);
    }
}

#endif //DRAG_DROP_H
