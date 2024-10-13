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
#ifndef ARRAY_H
#define ARRAY_H

#include "defines.h"
#include <stdlib.h>
#include <iterator>

template<typename T>
struct Array {
    static constexpr u32 CHUNK_BYTES = 4096;
    T *data;
    u32 count;
    u32 capacity;
    
    INLINE T *begin() {return data;}
    INLINE T const* cbegin() const {return data;}
    INLINE T *end() {return &data[count-1];}
    INLINE T const* cend() {return &data[count-1];};
    
    INLINE T& operator[](i64 index) {
        ASSERT(index >= 0 && index < (i64)count);
        return data[index];
    }
    INLINE T const& operator[](i64 index) const {
        ASSERT(index >= 0 && index < (i64)count);
        return data[index];
    }
    
    INLINE T& get(i32 index) {
        ASSERT(index >= 0 && index < count);
        return data[index];
    }
    
    INLINE T const& get(i32 index) const {
        ASSERT(index >= 0 && index < count);
        return data[index];
    }
    
    INLINE u32 push(u32 n = 1) {
        u32 offset = count;
        count += n;
        if (count >= capacity) {
            while (count >= capacity) capacity += CHUNK_BYTES/sizeof(T);\
            data = (T*)realloc(data, capacity * sizeof(T));
        }
        return offset;
    }
    
    INLINE u32 append(T const& e) {
        u32 index = this->push(1);
        this->data[index] = e;
        return index;
    }
    
    INLINE u32 append_array(T const *items, u32 item_count) {
        u32 offset = this->push(item_count);
        //memcpy(&data[offset], items, item_count * sizeof(T));
        for (u32 i = 0; i < item_count; ++i) {
            data[offset+i] = items[i];
        }
        return offset;
    }
    
    INLINE bool contains(T const& e) const {
        i32 index = linear_search(data, count, e);
        return index >= 0;
    }
    
    INLINE i32 lookup(T const& e) {
        return linear_search(data, count, e);
    }
    
    INLINE u32 lookup_or_append(T const& e) {
        i32 index = linear_search(data, count, e);
        if (index != -1) return index;
        return append(e);
    }
    
    INLINE bool append_unique(T const& e) {
        if (!contains(e)) {append(e); return true;}
        return false;
    }
    
    INLINE void copy_range_to(u32 first, u32 last, Array<T>& dst) const {
        for (u32 i = first; i <= last; ++i) dst.append(data[i]);
    }
    
    INLINE void copy_unique_range_to(u32 first, u32 last, Array<T>& dst) const {
        for (u32 i = first; i <= last; ++i) dst.append_unique(data[i]);
    }
    
    INLINE void copy_to(Array<T>& dst) const {
        if (count) copy_range_to(0, count-1, dst);
    }
    
    INLINE void copy_unique_to(Array<T>& dst) const {
        if (count) copy_unique_range_to(0, count-1, dst);
    }
    
    INLINE void ordered_remove(u32 index) {
        ASSERT(count);
        ASSERT(index < count);
        if ((index+1) == count) {
            count--;
            return;
        }
        
        T *copy_src = &data[index+1];
        T *copy_dst = &data[index];
        u32 copy_count = count - index - 1;
        
        memmove(copy_dst, copy_src, copy_count * sizeof(T));
        count--;
    }
    
    INLINE void pull(u32 n = 1) {
        ASSERT(count >= n);
        count -= n;
    }
    
    INLINE T pop() {
        ASSERT(count > 0);
        count--;
        return data[count];
    }
    
    INLINE void clear() {
        count = 0;
    }
    
    INLINE void free() {
        ::free(data);
        data = NULL;
        capacity = 0;
        count = 0;
    }
};

#endif //ARRAY_H

