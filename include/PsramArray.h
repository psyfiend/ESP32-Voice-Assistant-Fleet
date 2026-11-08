// A fixed array whose storage is taken from PSRAM the first time it is used.
//
// Large fixed arrays do not belong in internal RAM (LESSONS): on the S3s that keep
// LVGL's pool there, 2.13's window buffers alone pushed .dram0.bss past the end of
// internal RAM. Declared in place of `static T name[N];`, it is used the same way -
// `name[i]`, or `name` where a `T *` is wanted - and costs one pointer until then.
//
// T must be plain data: the storage is zero-filled, as a static array's is, and no
// constructor runs. Falls back to the internal heap if PSRAM has no room.
#pragma once
#include <stddef.h>
#include <stdlib.h>
#include <esp_heap_caps.h>

template <typename T, size_t N> struct PsramArray {
    T *p = nullptr;
    T *data() {
        if (!p) {
            p = (T *)heap_caps_calloc(N, sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (!p) p = (T *)calloc(N, sizeof(T));
        }
        return p;
    }
    // No operator[]: with this conversion present it would make `a[i]` ambiguous.
    operator T *() { return data(); }
    static constexpr size_t size() { return N; }
};
