#pragma once
//
// esp_async_fbcpy - ESP-IDF's DMA2D frame-buffer copy, which our prebuilt
// libesp_lcd.a CONTAINS (esp_async_fbcpy.c.obj, defines all three functions)
// but whose header IDF keeps private (components/esp_lcd/priv_include/), so
// the framework does not ship it. esp_lvgl_adapter reaches it by adding that
// private include path; we have no IDF tree, so the declarations are copied
// here VERBATIM from ESP-IDF commit 2553c5ad432 - the exact commit our rebuilt
// P4 libraries report (System Doctor, [FIRMWARE]: v5.5.5-832-g2553c5ad432).
// They must match the compiled library byte for byte: if the P4 libraries are
// ever rebuilt from another IDF commit, re-check this file against it.
//
// SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
// SPDX-License-Identifier: Apache-2.0
//
// Behaviour, read from esp_lcd/src/esp_async_fbcpy.c at the same commit:
//   - AT MOST ONE COPY OUTSTANDING IN THE WHOLE PROGRAM, whatever the number
//     of handles. esp_async_fbcpy() passes the DMA2D driver a pointer to ONE
//     `static dma2d_trans_config_t` shared by every handle, and the driver
//     reads it only when a QUEUED job starts - by then a later call has
//     rewritten it, so the job runs with the last caller's handle: pieces
//     copied twice, others never. Found 2026-09-26 as stale pixels on
//     WS_P4_5 with a 16-handle pool; two copies that both START at once (free
//     channels, nothing queued) happen to survive, which is why a two-copy
//     test passed. LVGL_Flush_EspLcd.cpp runs every copy through one worker.
//   - ONE transaction in flight per handle (a single descriptor pair); start
//     the next only after the callback for the previous one.
//   - Runs on a DMA2D channel pair from the shared pool - the PPA uses the same
//     engine but its own channels, so the two can run at the same time.
//   - The callback runs in the DMA2D interrupt.
//   - It syncs its own descriptors, NOT the data buffers: cache coherence of
//     source and destination is the caller's job.
//
#include "esp_err.h"
#include "hal/color_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct esp_async_fbcpy_context_t *esp_async_fbcpy_handle_t;

typedef struct {
} esp_async_fbcpy_config_t;

esp_err_t esp_async_fbcpy_install(const esp_async_fbcpy_config_t *config, esp_async_fbcpy_handle_t *mcp);
esp_err_t esp_async_fbcpy_uninstall(esp_async_fbcpy_handle_t mcp);

typedef struct {
    const void *src_buffer;   /*!< Source buffer */
    void *dst_buffer;         /*!< Destination buffer */
    size_t src_buffer_size_x; /*!< Source buffer size in x direction, size count in the number of pixels */
    size_t src_buffer_size_y; /*!< Source buffer size in y direction, size count in the number of pixels */
    size_t dst_buffer_size_x; /*!< Destination buffer size in x direction, size count in the number of pixels */
    size_t dst_buffer_size_y; /*!< Destination buffer size in y direction, size count in the number of pixels */
    size_t src_offset_x;      /*!< Copy action will start from this offset in source buffer in the x direction, offset count in the number of pixels */
    size_t src_offset_y;      /*!< Copy action will start from this offset in source buffer in the y direction, offset count in the number of pixels */
    size_t dst_offset_x;      /*!< Copy action will start from this offset in destination buffer in the x direction, offset count in the number of pixels */
    size_t dst_offset_y;      /*!< Copy action will start from this offset in destination buffer in the y direction, offset count in the number of pixels */
    size_t copy_size_x;       /*!< Copy size in the x direction, size count in the number of pixels */
    size_t copy_size_y;       /*!< Copy size in the y direction, size count in the number of pixels */
    color_space_pixel_format_t pixel_format_unique_id; /*!< Pixel format unique ID */
} esp_async_fbcpy_trans_desc_t;

typedef struct {
} esp_async_fbcpy_event_data_t;

typedef bool (*esp_async_fbcpy_event_callback_t)(esp_async_fbcpy_handle_t mcp, esp_async_fbcpy_event_data_t *event_data, void *cb_args);

esp_err_t esp_async_fbcpy(esp_async_fbcpy_handle_t mcp, esp_async_fbcpy_trans_desc_t* transaction,
                          esp_async_fbcpy_event_callback_t memcpy_done_cb, void *cb_args);

#ifdef __cplusplus
}
#endif
