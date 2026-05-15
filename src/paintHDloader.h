/*
 * PaintHD loader
 * Copyright (c) 2026 WojciechGw
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PAINT_HD_LOADER_H
#define PAINT_HD_LOADER_H

#include <rp6502.h>
#include <6502.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define XRAM_GFXASSETS_ADDR 0xEC00u

#define font ascii_font_5x7
#include "font5x7.h"
#undef font

#include "icons.h"

#define BLACK 0
#define WHITE 1

#define CANVAS_DATA          0x0000
#define POINTER_DATA         0xEA00

#define CANVAS_STRUCT        0xFF00
#define POINTER_STRUCT       0xFF20

#define POINTER_WIDTH  15
#define POINTER_HEIGHT 15

#define KEYBOARD_INPUT       0xFF80
#define MOUSE_INPUT          0xFFA0
#define MOUSE_INPUT_BUTTONS  (MOUSE_INPUT)
#define MOUSE_INPUT_X        (MOUSE_INPUT + 1)
#define MOUSE_INPUT_Y        (MOUSE_INPUT + 2)

#define CANVAS_STRIDE 80u

#define GFX_CANVAS_640x480 3
#define GFX_CANVAS_TYPE GFX_CANVAS_640x480
#define GFX_CANVAS_WIDTH  640u
#define GFX_CANVAS_HEIGHT 480u

#define GFX_MODE_BITMAP    3
#define GFX_PLANE_0 0
#define GFX_PLANE_2 2
#define GFX_BITMAP_bpp1    0b00000000
#define GFX_BITMAP_bpp8    0b00000011

#define STARTUP_TILE_COLS 4u
#define STARTUP_TILE_ROWS 4u
#define STARTUP_TILE_COUNT (STARTUP_TILE_COLS * STARTUP_TILE_ROWS)
#define STARTUP_TOP_MARGIN 16u
#define STARTUP_TILE_WIDTH 160u
#define STARTUP_TILE_HEIGHT 116u
#define STARTUP_SOURCE_HEIGHT (GFX_CANVAS_HEIGHT - STARTUP_TOP_MARGIN)
#define STARTUP_LABEL_HEIGHT 10u
#define STARTUP_CACHE_PATH_LEN 20u

#define HID_PAGEUP 0x4Bu
#define HID_PAGEDOWN 0x4Eu

#define PAUSE_TICKS_START 200u

uint32_t ticks = 0;
#define PAUSE(millis) ticks=clock(); while(clock() < (ticks + millis)){}
#define key_pressed(code) \
    (RIA.addr0 = KEYBOARD_INPUT + ((code) >> 3), RIA.step0 = 0, \
    ((RIA.rw0 & (1u << ((code) & 7))) != 0u))

void *__fastcall__ argv_mem(size_t size);

static void mouse_init(void);
static void draw_pointer(uint8_t cursor);
static void fill_canvas(uint8_t color);
static void clear_canvas_random_blocks8(void);
static void raw_set_pixel(int x, int y, uint8_t color);
static void startup_enter_program_dir(const char *argv0);
static const char *startup_basename(const char *path);
static char *startup_make_path(const char *dir, const char *name);
static uint8_t startup_bmp_name_match(const char *name);
static void startup_clear_bmps(void);
static uint8_t startup_ensure_capacity(unsigned count);
static void startup_insert_bmp(const char *name, unsigned fdate, unsigned ftime);
static uint8_t startup_collect_bmps(void);
static unsigned startup_page_base(void);
static void startup_page_cache_path(unsigned page, char *out);
static void startup_clear_page_cache(void);
static uint8_t startup_load_page_cache(unsigned page);
static void startup_save_page_cache(unsigned page);
static int text_width(const char *text);
static void draw_text_char(char ch, int px, int py, uint8_t fg_color, uint8_t bg_color);
static void draw_text(const char *text, int px, int py, int max_x, uint8_t fg_color, uint8_t bg_color);
static void draw_header_bar(void);
static void draw_tile(unsigned index);
static void draw_tiles(void);
static int tile_hit(int x, int y);
static int wait_for_selection(void);

#endif /* PAINT_HD_LOADER_H */
