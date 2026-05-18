/*
 * PaintHD Zoom editor
 * Copyright (c) 2026 WojciechGw
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PAINT_HD_ZOOM_H
#define PAINT_HD_ZOOM_H

#include <rp6502.h>
#include <6502.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "bmp.h"

#define XRAM_GFXASSETS_ADDR 0xEC00u

#define font ascii_font_5x7
#include "font5x7.h"
#undef font

#include "icons.h"

#define CANVAS_DATA     0x0000
#define POINTER_DATA    0xEA00
#define CANVAS_STRUCT   0xFF00
#define POINTER_STRUCT  0xFF20

#define POINTER_WIDTH  15
#define POINTER_HEIGHT 15

#define KEYBOARD_INPUT      0xFF80
#define MOUSE_INPUT         0xFFA0
#define MOUSE_INPUT_BUTTONS (MOUSE_INPUT)
#define MOUSE_INPUT_X       (MOUSE_INPUT + 1)
#define MOUSE_INPUT_Y       (MOUSE_INPUT + 2)

#define CANVAS_STRIDE 80u

#define GFX_CANVAS_640x480 3

#define GFX_MODE_BITMAP  3
#define GFX_PLANE_0      0
#define GFX_PLANE_2      2
#define GFX_BITMAP_bpp1  0b00000000
#define GFX_BITMAP_bpp8  0b00000011
#define GFX_CANVAS_WIDTH  640u
#define GFX_CANVAS_HEIGHT 480u

#define ZOOM_AREA_W    64
#define ZOOM_AREA_H    48
#define ZOOM_DOT       8
#define ZOOM_VIEW_W    512   /* 64 * 8 */
#define ZOOM_VIEW_H    384   /* 48 * 8 */
#define ZOOM_VIEW_X0   64    /* ZOOM_FRAME_X0 + ZOOM_DOT */
#define ZOOM_VIEW_Y0   48    /* ZOOM_FRAME_Y0 + ZOOM_DOT */
#define ZOOM_FRAME_X0  56    /* 7*8, byte-aligned, (640-528)/2 */
#define ZOOM_FRAME_Y0  40    /* (480-400)/2 */
#define ZOOM_FRAME_W   528   /* 66*8: 64 pixel blocks + 1 frame block each side */
#define ZOOM_FRAME_H   400   /* 50*8: 48 pixel blocks + 1 frame block each side */
#define ZOOM_FRAME_X0_BYTE  (ZOOM_FRAME_X0 / 8u)   /* = 7u */
#define ZOOM_AREA_BUF_ADDR  0x9600u  /* canvas area backup: 64*48/8=384 B (1bpp) */
#define ZOOM_BUF_ADDR       0x9780u  /* edit scratch:       64*48/8=384 B (1bpp) */

#define HID_ESCAPE     0x29
#define HID_ENTER      0x28
#define HID_LEFT_CTRL  0xE0
#define HID_LEFT_SHIFT 0xE1

#define key_pressed(code) \
    (RIA.addr0 = KEYBOARD_INPUT + ((code) >> 3), RIA.step0 = 0, \
     RIA.rw0 & (1 << ((code) & 7)))

void *__fastcall__ argv_mem(size_t size);

unsigned char mouse_irq_fn(void);
static void mouse_init(void);
static void draw_pointer(uint8_t type);
static void raw_set_pixel(int x, int y, uint8_t color);
static void raw_toggle_pixel(int x, int y);
static uint8_t snapshot_save_canvas(const char *path);
static uint8_t snapshot_load_canvas(const char *path);
static void zoom_area_pixels_save(void);
static void zoom_draw_view(void);
static void zoom_redraw_block(int sx, int sy);
static void zoom_apply_changes(void);
static void draw_canvas_text_char(char ch, int px, int py, uint8_t fg, uint8_t bg);
static void draw_canvas_text(const char *text, int x, int y, int max_x,
                             uint8_t fg, uint8_t bg);

#endif /* PAINT_HD_ZOOM_H */
