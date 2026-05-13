/*
 * PaintHD
 * Copyright (c) 2026 WojciechGw
 * based on paint.c example
 * Copyright (c) 2025 Rumbledethumps
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PAINT_HD_H
#define PAINT_HD_H

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
#include "bmp.h"

#define font ascii_font_5x7
#include "font5x7.h"
#undef font

#define ICONS_FULLSET
#include "icons.h"

#define PATHS_APP "./"
#define PATHS_TMP PATHS_APP "TMP/"
#define FILES_DEFAULT_SNAPSHOT "paintHD_"
#define PATHS_SNAPSHOT PATHS_TMP FILES_DEFAULT_SNAPSHOT

#define CANVAS_DATA          0x0000
#define PICKER_DATA          0xA000
#define POINTER_DATA         0xEA00

#define CANVAS_STRUCT        0xFF00
#define PICKER_STRUCT        0xFF10
#define POINTER_STRUCT       0xFF20

#define PICKER_HEIGHT 20
#define PICKER_BUTTON_SIZE 16
#define PICKER_GAP 1

#define PICKER_HANDLE_X 1
#define PICKER_TITLE_X  (PICKER_HANDLE_X + PICKER_BUTTON_SIZE + PICKER_GAP)
#define PICKER_TITLE_WIDTH 65
#define PICKER_SAVE_X   (PICKER_TITLE_X + PICKER_TITLE_WIDTH)
#define PICKER_SAVE_SPACER 8
#define PICKER_ERASE_SPACER 8
#define PICKER_SWAP_X   (PICKER_SAVE_X + PICKER_BUTTON_SIZE + PICKER_SAVE_SPACER)
#define PICKER_ERASE_X  (PICKER_SWAP_X + PICKER_BUTTON_SIZE)
#define PICKER_INVERT_X (PICKER_ERASE_X + PICKER_BUTTON_SIZE)
#define PICKER_MIRROR_X (PICKER_INVERT_X + PICKER_BUTTON_SIZE)
#define PICKER_SELECT_X (PICKER_MIRROR_X + PICKER_BUTTON_SIZE + PICKER_ERASE_SPACER)
#define PICKER_RECT_X   (PICKER_SELECT_X + PICKER_BUTTON_SIZE)
#define PICKER_ELLIPSE_X (PICKER_RECT_X + PICKER_BUTTON_SIZE)
#define PICKER_PRIMITIVE_SPACER 8
#define PICKER_SHAPE0_X (PICKER_ELLIPSE_X + PICKER_BUTTON_SIZE + PICKER_PRIMITIVE_SPACER)
#define PICKER_MINUS_X  (PICKER_SHAPE0_X + SHAPE_COUNT * PICKER_BUTTON_SIZE)
#define PICKER_SIZE_X   (PICKER_MINUS_X + PICKER_BUTTON_SIZE)
#define PICKER_PLUS_X   (PICKER_SIZE_X + PICKER_BUTTON_SIZE)
#define PICKER_COORDS_X (PICKER_PLUS_X + PICKER_BUTTON_SIZE + PICKER_GAP)
#define PICKER_COORDS_WIDTH 64
#define PICKER_STATUS_X (PICKER_COORDS_X + PICKER_COORDS_WIDTH)
#define PICKER_STATUS_WIDTH 162
#define PICKER_WIDTH  (PICKER_STATUS_X + PICKER_STATUS_WIDTH + 1)

#define POINTER_WIDTH  15
#define POINTER_HEIGHT 15

#define KEYBOARD_INPUT       0xFF80

#define MOUSE_INPUT          0xFFA0
#define MOUSE_INPUT_BUTTONS  (MOUSE_INPUT)
#define MOUSE_INPUT_X        (MOUSE_INPUT + 1)
#define MOUSE_INPUT_Y        (MOUSE_INPUT + 2)
#define MOUSE_INPUT_WHEEL    (MOUSE_INPUT + 3)
#define MOUSE_INPUT_PAN      (MOUSE_INPUT + 4)
#define MOUSE_DIV 1 // lower canvas resolution > higher MOUSE_DIV value

#define CANVAS_STRIDE 80u

#define HID_A 0x04
#define HID_C 0x06
#define HID_S 0x16
#define HID_ESCAPE 0x29
#define HID_F2 0x3B
#define HID_F3 0x3C
#define HID_F4 0x3D
#define HID_F10 0x43
#define HID_V 0x19
#define HID_X 0x1B
#define HID_Y 0x1C
#define HID_Z 0x1D
#define HID_LEFT_CTRL 0xE0
#define HID_LEFT_SHIFT 0xE1
#define HID_LEFT_ALT 0xE2
#define HID_RIGHT_SHIFT 0xE5
#define HID_RIGHT_ALT 0xE6
#define shift_pressed() \
    (key_pressed(HID_LEFT_SHIFT) || key_pressed(HID_RIGHT_SHIFT))
#define alt_pressed() \
    (key_pressed(HID_LEFT_ALT) || key_pressed(HID_RIGHT_ALT))
#define key_pressed(code) \
    (RIA.addr0 = KEYBOARD_INPUT + ((code) >> 3), RIA.step0 = 0, \
     RIA.rw0 & (1 << ((code) & 7)))

#define BRUSH_MIN 1
#define BRUSH_MAX 64

#define TOOL_BRUSH  0
#define TOOL_SELECT 1
#define TOOL_RECT   2
#define TOOL_ELLIPSE 3

#define DRAW_BUTTON_NONE  0u
#define DRAW_BUTTON_LEFT  1u
#define DRAW_BUTTON_RIGHT 2u

#define UNDO_LEVELS 255u
#define TMP_PATH_LEN 35u

#define DOUBLE_CLICK_TICKS ((CLOCKS_PER_SEC / 3) ? (CLOCKS_PER_SEC / 3) : 1)
#define PICKER_STATUS_TICKS ((CLOCKS_PER_SEC * 5) ? (CLOCKS_PER_SEC * 3) : 1)
#define PICKER_PANEL_COLOR 239u
#define PICKER_MID_COLOR 160u
#define PICKER_FG_COLOR 255u
#define PICKER_BG_COLOR 245u
#define PICKER_SAVE_DISABLED_COLOR 245u

#define SHAPE_COUNT  6
#define SHAPE_SQUARE 0
#define SHAPE_CIRCLE 1
#define SHAPE_VERT   2
#define SHAPE_DIAG   3
#define SHAPE_SPRAY  4
#define SHAPE_FILL   5

#define FILL_QUEUE_SIZE 1024 //2048

#define GFX_CANVAS_CONSOLE 0
#define GFX_CANVAS_320x240 1
#define GFX_CANVAS_320x180 2
#define GFX_CANVAS_640x480 3
#define GFX_CANVAS_640x360 4

#define GFX_MODE_CONSOLE   0
#define GFX_MODE_CHARACTER 1
#define GFX_MODE_TILE      2
#define GFX_MODE_BITMAP    3
#define GFX_MODE_SPRITE    4

#define GFX_MODE_SPRITE_NORMAL 0b00000000
#define GFX_MODE_SPRITE_AFFINE 0b00000010

#define GFX_BITMAP_bpp1    0b00000000
#define GFX_BITMAP_bpp2    0b00000001
#define GFX_BITMAP_bpp4    0b00000010
#define GFX_BITMAP_bpp8    0b00000011
#define GFX_BITMAP_bpp16   0b00000100
#define GFX_BITMAP_REVERSE 0b00001000

#define GFX_CHARACTER_bpp1         0b00000000
#define GFX_CHARACTER_bpp4         0b00000010
#define GFX_CHARACTER_bpp4_REVERSE 0b00000001
#define GFX_CHARACTER_bpp8         0b00000011
#define GFX_CHARACTER_bpp16        0b00000100

#define GFX_FONT_CUSTOM 0xF000        // custom font
#define GFX_CHARACTER_FONT_PTR 0xFFFF // system font
#define GFX_CHARACTER_PAL_PTR  0xFFFF

#define GFX_PLANE_0 0
#define GFX_PLANE_1 1
#define GFX_PLANE_2 2

#define GFX_CHARACTER_FONTSIZE8x16 0b00001000
#define GFX_CHARACTER_FONTSIZE8x8  0b00000000

#define GFX_CANVAS_TYPE GFX_CANVAS_640x480
#define GFX_CANVAS_WIDTH  640u
#define GFX_CANVAS_HEIGHT 480u
/*
#define GFX_CANVAS_COLUMNS (GFX_CANVAS_WIDTH / 8u)
#define GFX_CANVAS_ROWS (GFX_CANVAS_HEIGHT / 8u)
#define GFX_CANVAS_BYTES_PER_CHARACTER 6u
*/

#define GFX_CANVAS_TYPE GFX_CANVAS_640x480

#define COLOR_FROM_RGB8(r,g,b) (((b>>3)<<11)|((g>>3)<<6)|(r>>3))
#define COLOR_FROM_RGB5(r,g,b) ((b<<11)|(g<<6)|(r))
#define COLOR_ALPHA_MASK (1u<<5)

// time related
uint32_t ticks = 0; // for PAUSE(millis)
#define PAUSE(millis) ticks=clock(); while(clock() < (ticks + millis)){}

/* Forward declarations used by paintHD.c */
static void draw_save_button(void);
static void draw_invert_button(void);
static void draw_mirror_button(void);
static void perform_mirror_action(void);
static void set_canvas_dirty(bool dirty);
static void canvas_modify_begin(void);
static void canvas_modify_end(void);
static void clear_canvas_random_pixels(void);
static void clear_canvas_random_blocks8(void);
static void operation_cancel_begin(void);
static uint8_t operation_cancel_requested(void);
static uint8_t operation_was_cancelled(void);
static void set_picker_status(const char *text);
static void fill_rect(int x1, int y1, int x2, int y2, uint8_t color);
static void set_pixel(int x, int y, uint8_t color);
static char *append_coord_value(char *out, int value);
static void invert_canvas_region(void);
static void mirror_canvas_region(void);
static uint8_t reverse_bits8(uint8_t value);
static void mirror_canvas_full_vertical(void);
static void mirror_canvas_full_horizontal(void);
static uint8_t selection_mode_active(void);
static void selection_hide_overlay(void);
static void selection_show_overlay(void);
static void paste_preview_hide(void);
static void paste_preview_show(void);
static void primitive_hide_overlay(void);
static void primitive_show_overlay(void);
static void snapshot_path(char *out, char kind, uint8_t index);
static uint8_t snapshot_save_canvas(const char *path);
static uint8_t snapshot_load_canvas(const char *path);
static void snapshot_stack_clear(char kind, uint8_t *count);
static uint8_t snapshot_stack_push(char kind, uint8_t *dirty_stack, uint8_t *count, uint8_t dirty_value);
static uint8_t snapshot_stack_pop(char kind, uint8_t *dirty_stack, uint8_t *count, uint8_t *dirty_value);
static uint8_t snapshot_stage_current(char kind, uint8_t *dirty_stack, uint8_t *count, uint8_t dirty_value);
static uint8_t snapshot_refresh_current(void);
static uint8_t prepare_undo_step(void);
static void perform_undo(void);
static void perform_redo(void);
static void drawing_session_begin(void);
static void drawing_session_end(void);
static uint8_t primitive_mode_active(void);
static void toggle_mirror_mode(void);
static const char *mirror_status_text(void);
static void primitive_apply_constraint(int anchor_x, int anchor_y, int *x, int *y);
static void draw_brush_shape(uint8_t shape, int x, int y);
static void draw_line_brush_shape(uint8_t shape, int x0, int y0, int x1, int y1);
static uint8_t draw_line_square_fast(int x0, int y0, int x1, int y1);
static uint8_t draw_line_vert_fast(int x0, int y0, int x1, int y1);
static uint8_t primitive_tools_enabled(void);
static uint8_t brush_shape_enabled(uint8_t shape);
static uint8_t primitive_stroke_shape(void);
static void primitive_update_rect(int x1, int y1, int x2, int y2);
static void primitive_store_ellipse_point(int x, int y);
static void primitive_build_ellipse_rows(void);
static void primitive_toggle_point_overlay(void);
static void primitive_toggle_rect_overlay(void);
static void primitive_toggle_ellipse_overlay(void);
static void primitive_toggle_overlay(void);
static void update_primitive_size_status(void);
static void primitive_begin_drag(uint8_t tool, uint8_t button, uint8_t color, int x, int y);
static void primitive_update_drag(int x, int y);
static void primitive_draw_rect_shape(uint8_t shape, int x1, int y1, int x2, int y2);
static void primitive_draw_ellipse_shape(uint8_t shape, int x0, int y0, int x1, int y1);
static void primitive_finish_drag(void);
static void primitive_cancel(void);
static void draw_rect_tool_button(void);
static void draw_ellipse_tool_button(void);
static void exit_primitive_mode(void);

#endif /* PAINT_HD_H */
