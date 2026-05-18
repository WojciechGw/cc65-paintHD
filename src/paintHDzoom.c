/*
 * PaintHD Zoom editor
 * Copyright (c) 2026 WojciechGw
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Launched by paintHDeditor via:
 *   ria_execl("paintHDzoom.rp6502", bmp_name, x_str, y_str, undo_str, redo_str, NULL)
 *
 * Returns to paintHDeditor via:
 *   ria_execl("paintHDeditor.rp6502", bmp_name, "--from-zoom", undo_str, redo_str, NULL)
 *
 * Canvas backup is in TMP/paintHD_zoom.bin (written by editor before exec).
 * On ENTER: zoom writes modified canvas back to TMP/paintHD_zoom.bin, then execs editor.
 * On ESCAPE: zoom execs editor without modifying TMP/paintHD_zoom.bin.
 */

#include "paintHDzoom.h"

void *__fastcall__ argv_mem(size_t size)
{
    static uint8_t buf[512];
    if (size > sizeof(buf)) return NULL;
    return buf;
}

/* ── state ─────────────────────────────────────────────────────────────── */

static int16_t mouse_pos_x;
static int16_t mouse_pos_y;
static uint8_t mouse_stack[8];

static int     zoom_area_x;
static int     zoom_area_y;
static uint8_t zoom_paint_armed;
static uint8_t zoom_paint_value;
static int     zoom_paint_last_sx;
static int     zoom_paint_last_sy;

static char    bmp_name[64];
static char    undo_str[4];
static char    redo_str[4];

/* ── mouse IRQ ──────────────────────────────────────────────────────────── */

unsigned char mouse_irq_fn(void)
{
    static int16_t raw_x, raw_y;
    static uint8_t prev_x, prev_y;
    uint8_t rw;
    uint8_t save_step0;
    uint16_t save_addr0;

    VIA.ifr = 0x40;
    save_addr0 = RIA.addr0;
    save_step0 = RIA.step0;

    RIA.addr0 = MOUSE_INPUT_X;
    RIA.step0 = 0;
    rw = RIA.rw0;
    raw_x += (int8_t)(rw - prev_x);
    prev_x = rw;
    if (raw_x < 0) raw_x = 0;
    if (raw_x >= (int)GFX_CANVAS_WIDTH) raw_x = (int)GFX_CANVAS_WIDTH - 1;

    RIA.addr0 = MOUSE_INPUT_Y;
    rw = RIA.rw0;
    raw_y += (int8_t)(rw - prev_y);
    prev_y = rw;
    if (raw_y < 0) raw_y = 0;
    if (raw_y >= (int)GFX_CANVAS_HEIGHT) raw_y = (int)GFX_CANVAS_HEIGHT - 1;

    mouse_pos_x = raw_x;
    mouse_pos_y = raw_y;
    xram0_struct_set(POINTER_STRUCT, vga_mode3_config_t, x_pos_px, mouse_pos_x - 7);
    xram0_struct_set(POINTER_STRUCT, vga_mode3_config_t, y_pos_px, mouse_pos_y - 7);

    RIA.addr0 = save_addr0;
    RIA.step0 = save_step0;
    return IRQ_HANDLED;
}

static void mouse_init(void)
{
    unsigned timer_val = (ria_attr_get(RIA_ATTR_PHI2_KHZ) * 8u) - 2u;
    VIA.t1l_lo = timer_val & 0xFF;
    VIA.t1l_hi = timer_val >> 8;
    VIA.t1_lo  = timer_val & 0xFF;
    VIA.t1_hi  = timer_val >> 8;
    VIA.acr = 0x40;
    VIA.ier = 0xC0;
    xreg_ria_mouse(MOUSE_INPUT);
    set_irq(mouse_irq_fn, &mouse_stack, sizeof(mouse_stack));
}

/* ── pointer ────────────────────────────────────────────────────────────── */

static void draw_pointer(uint8_t type)
{
    switch (type) {
    case POINTER_arrow:
        xram0_struct_set(POINTER_STRUCT, vga_mode3_config_t, xram_data_ptr, XRAM_POINTERS_arrow);
        break;
    case POINTER_hourglass:
        xram0_struct_set(POINTER_STRUCT, vga_mode3_config_t, xram_data_ptr, XRAM_POINTERS_hourglass);
        break;
    }
}

/* ── canvas helpers ─────────────────────────────────────────────────────── */

static void raw_set_pixel(int x, int y, uint8_t color)
{
    uint8_t mask;
    if (x < 0 || x >= (int)GFX_CANVAS_WIDTH || y < 0 || y >= (int)GFX_CANVAS_HEIGHT)
        return;
    RIA.step0 = 0;
    RIA.addr0 = CANVAS_DATA + (unsigned)y * CANVAS_STRIDE + (unsigned)x / 8u;
    mask = (uint8_t)(0x80u >> (x & 7));
    if (color) RIA.rw0 = RIA.rw0 | mask;
    else       RIA.rw0 = RIA.rw0 & (uint8_t)~mask;
}

/* ── snapshot ───────────────────────────────────────────────────────────── */

static uint8_t snapshot_save_canvas(const char *path)
{
    uint16_t addr;
    uint16_t remaining;
    unsigned chunk;
    int fd;

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) return 0;
    addr = CANVAS_DATA;
    remaining = (uint16_t)(GFX_CANVAS_HEIGHT * CANVAS_STRIDE);
    while (remaining > 0)
    {
        chunk = (remaining > 0x7FFFu) ? 0x7FFFu : (unsigned)remaining;
        if (write_xram(addr, chunk, fd) < 0) { close(fd); return 0; }
        addr      += (uint16_t)chunk;
        remaining -= (uint16_t)chunk;
    }
    close(fd);
    return 1u;
}

static uint8_t snapshot_load_canvas(const char *path)
{
    uint16_t addr;
    uint16_t remaining;
    unsigned chunk;
    int fd;

    fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    addr = CANVAS_DATA;
    remaining = (uint16_t)(GFX_CANVAS_HEIGHT * CANVAS_STRIDE);
    while (remaining > 0)
    {
        chunk = (remaining > 0x7FFFu) ? 0x7FFFu : (unsigned)remaining;
        if (read_xram(addr, chunk, fd) != (int)chunk) { close(fd); return 0; }
        addr      += (uint16_t)chunk;
        remaining -= (uint16_t)chunk;
    }
    close(fd);
    return 1u;
}

/* ── canvas text ────────────────────────────────────────────────────────── */

static void draw_canvas_text_char(char ch, int px, int py, uint8_t fg, uint8_t bg)
{
    unsigned char code = (unsigned char)ch;
    int row, col;
    uint8_t bits, mask;

    for (col = 0; col < 6; col++)
        raw_set_pixel(px + col, py - 1, bg);
    for (row = 0; row < 8; row++)
    {
        mask = (uint8_t)(1u << row);
        for (col = 0; col < 5; col++)
        {
            RIA.step0 = 0;
            RIA.addr0 = XRAM_FONT5x7_ADDR + (unsigned)code * XRAM_FONT5x7_GLYPH_SIZE + (unsigned)col;
            bits = RIA.rw0;
            raw_set_pixel(px + col, py + row, (bits & mask) ? fg : bg);
        }
        raw_set_pixel(px + 5, py + row, bg);
    }
    for (col = 0; col < 6; col++)
        raw_set_pixel(px + col, py + 8, bg);
}

static void draw_canvas_text(const char *text, int px, int py, int max_x,
                             uint8_t fg, uint8_t bg)
{
    while (*text != '\0' && (px + 4) <= max_x)
    {
        draw_canvas_text_char(*text, px, py, fg, bg);
        px += 6;
        text++;
    }
}

/* ── zoom functions ─────────────────────────────────────────────────────── */

static void zoom_area_pixels_save(void)
{
    int sy, bx;
    unsigned src_byte;
    uint8_t byte_val;
    unsigned stride = ZOOM_AREA_W / 8u;   /* bytes per row in 1bpp buffers */

    for (sy = 0; sy < ZOOM_AREA_H; sy++)
    {
        src_byte = CANVAS_DATA
                   + (unsigned)zoom_area_y * CANVAS_STRIDE
                   + (unsigned)zoom_area_x / 8u
                   + (unsigned)sy * CANVAS_STRIDE;
        for (bx = 0; bx < (int)stride; bx++)
        {
            RIA.step0 = 0;
            RIA.addr0 = src_byte + (unsigned)bx;
            byte_val = RIA.rw0;
            /* write to ZOOM_AREA_BUF (backup) */
            RIA.addr0 = ZOOM_AREA_BUF_ADDR + (unsigned)sy * stride + (unsigned)bx;
            RIA.rw0 = byte_val;
            /* write to ZOOM_BUF (edit scratch) */
            RIA.addr0 = ZOOM_BUF_ADDR + (unsigned)sy * stride + (unsigned)bx;
            RIA.rw0 = byte_val;
        }
    }
}

static void zoom_redraw_block(int sx, int sy)
{
    int py;
    uint8_t v, bv, byte_val, mask;
    unsigned row_addr;

    /* read 1 bit from 1bpp ZOOM_BUF */
    RIA.step0 = 0;
    RIA.addr0 = ZOOM_BUF_ADDR + (unsigned)sy * (ZOOM_AREA_W / 8u) + (unsigned)sx / 8u;
    byte_val = RIA.rw0;
    mask = (uint8_t)(0x80u >> (sx & 7));
    v = (byte_val & mask) ? 1u : 0u;

    for (py = 0; py < ZOOM_DOT; py++)
    {
        row_addr = (unsigned)(ZOOM_FRAME_Y0 + (sy + 1) * ZOOM_DOT + py)
                   * CANVAS_STRIDE + ZOOM_FRAME_X0_BYTE + 1u + (unsigned)sx;
        RIA.addr0 = row_addr;
        RIA.step0 = 0;
        bv = v ? 0xFF : 0x00;
        if (py == 1 || py == 3 || py == 5) bv ^= 0x01;
        if (py == 7) bv ^= 0x55;
        RIA.rw0 = bv;
    }
}

static void zoom_draw_view(void)
{
    int sx, sy, py, i;
    uint8_t v, byte_val;
    unsigned row_addr;

    /* render pixel blocks directly from 1bpp ZOOM_BUF */
    for (sy = -1; sy <= ZOOM_AREA_H; sy++)
    {
        for (py = 0; py < ZOOM_DOT; py++)
        {
            row_addr = (unsigned)(ZOOM_FRAME_Y0 + (sy + 1) * ZOOM_DOT + py)
                       * CANVAS_STRIDE + ZOOM_FRAME_X0_BYTE;
            RIA.addr0 = row_addr;
            RIA.step0 = 1;

            if (sy >= 0 && sy < ZOOM_AREA_H)
            {
                uint8_t b;
                RIA.rw0 = (py % 2);   /* left border byte */
                for (sx = 0; sx < ZOOM_AREA_W; sx++)
                {
                    if ((sx & 7) == 0)
                    {
                        RIA.step0 = 0;
                        RIA.addr0 = ZOOM_BUF_ADDR
                                    + (unsigned)sy * (ZOOM_AREA_W / 8u)
                                    + (unsigned)(sx / 8u);
                        b = RIA.rw0;
                        RIA.addr0 = row_addr + 1u + (unsigned)sx;
                        RIA.step0 = 0;
                    }
                    v = (b & (uint8_t)(0x80u >> (sx & 7u))) ? 1u : 0u;
                    byte_val = v ? 0xFF : 0x00;
                    if (py == 1 || py == 3 || py == 5) byte_val ^= 0b00000001;
                    if (py == 7) byte_val ^= 0b01010101;
                    RIA.rw0 = byte_val;
                    RIA.addr0++;
                }
                /* right border byte */
                RIA.addr0 = row_addr + 1u + ZOOM_AREA_W;
                RIA.step0 = 0;
                RIA.rw0 = (sy > 5 && py == 0 && !(sy % 8)) ? 0b11111111 : 0b00000000;
            }
            else
            {
                /* border rows: ZOOM_AREA_W+2 bytes */
                for (i = 0; i < ZOOM_AREA_W + 2; i++) {
                    if (sy == -1 && py < 7 && (i % ZOOM_DOT) == 0) {
                        RIA.rw0 = 0b00000001;
                    } else if (sy == -1 && py == 7) {
                        RIA.rw0 = (i >= 1 && i <= ZOOM_AREA_W) ? 0b01010101 : 0b00000001;
                    } else {
                        RIA.rw0 = 0b00000000;
                    }
                }
            }
        }
    }

    draw_canvas_text(" PaintHD > ZOOM mode ",
                     ZOOM_FRAME_X0, (ZOOM_FRAME_Y0 - 2u),
                     (int)(GFX_CANVAS_WIDTH - 1u), 0, 1);
    draw_canvas_text("LMB toggle pixel/s ENTER confirm changes ESC abandon changes",
                     ZOOM_FRAME_X0 + 8, (ZOOM_FRAME_Y0 + ZOOM_FRAME_H - 4),
                     (int)(GFX_CANVAS_WIDTH - 1u), 1, 0);
}

static void zoom_apply_changes(void)
{
    int sx, sy;
    uint8_t v;

    snapshot_load_canvas("TMP/paintHD_zoom.bin");
    for (sy = 0; sy < ZOOM_AREA_H; sy++)
    {
        uint8_t b;
        for (sx = 0; sx < ZOOM_AREA_W; sx++)
        {
            if ((sx & 7) == 0)
            {
                RIA.step0 = 0;
                RIA.addr0 = ZOOM_BUF_ADDR
                            + (unsigned)sy * (ZOOM_AREA_W / 8u)
                            + (unsigned)(sx / 8u);
                b = RIA.rw0;
            }
            v = (b & (uint8_t)(0x80u >> (sx & 7u))) ? 1u : 0u;
            raw_set_pixel(zoom_area_x + sx, zoom_area_y + sy, v);
        }
    }
}

/* ── main ───────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    uint8_t buttons;
    uint8_t prev_buttons;
    uint8_t prev_enter;
    uint8_t prev_escape;
    int x, y;

    if (argc < 4)
        return 1;

    strncpy(bmp_name, argv[1], sizeof(bmp_name) - 1);
    bmp_name[sizeof(bmp_name) - 1] = '\0';
    zoom_area_x = atoi(argv[2]);
    zoom_area_y = atoi(argv[3]);
    strncpy(undo_str, argc >= 5 ? argv[4] : "0", sizeof(undo_str) - 1);
    undo_str[sizeof(undo_str) - 1] = '\0';
    strncpy(redo_str, argc >= 6 ? argv[5] : "0", sizeof(redo_str) - 1);
    redo_str[sizeof(redo_str) - 1] = '\0';

    /* set up display — full reinit required after ria_execl */
    xreg_vga_canvas(GFX_CANVAS_640x480);

    xram0_struct_set(CANVAS_STRUCT, vga_mode3_config_t, x_wrap, false);
    xram0_struct_set(CANVAS_STRUCT, vga_mode3_config_t, y_wrap, false);
    xram0_struct_set(CANVAS_STRUCT, vga_mode3_config_t, x_pos_px, 0);
    xram0_struct_set(CANVAS_STRUCT, vga_mode3_config_t, y_pos_px, 0);
    xram0_struct_set(CANVAS_STRUCT, vga_mode3_config_t, width_px, GFX_CANVAS_WIDTH);
    xram0_struct_set(CANVAS_STRUCT, vga_mode3_config_t, height_px, GFX_CANVAS_HEIGHT);
    xram0_struct_set(CANVAS_STRUCT, vga_mode3_config_t, xram_data_ptr, CANVAS_DATA);
    xram0_struct_set(CANVAS_STRUCT, vga_mode3_config_t, xram_palette_ptr, 0xFFFF);

    xram0_struct_set(POINTER_STRUCT, vga_mode3_config_t, x_wrap, false);
    xram0_struct_set(POINTER_STRUCT, vga_mode3_config_t, y_wrap, false);
    xram0_struct_set(POINTER_STRUCT, vga_mode3_config_t, width_px, POINTER_WIDTH);
    xram0_struct_set(POINTER_STRUCT, vga_mode3_config_t, height_px, POINTER_HEIGHT);
    xram0_struct_set(POINTER_STRUCT, vga_mode3_config_t, xram_data_ptr, POINTER_DATA);
    xram0_struct_set(POINTER_STRUCT, vga_mode3_config_t, xram_palette_ptr, 0xFFFF);

    xreg_vga_mode(GFX_MODE_BITMAP, GFX_BITMAP_bpp1, CANVAS_STRUCT, GFX_PLANE_0);
    xreg_vga_mode(GFX_MODE_BITMAP, GFX_BITMAP_bpp8, POINTER_STRUCT, GFX_PLANE_2);
    xreg_ria_keyboard(KEYBOARD_INPUT);

    draw_pointer(POINTER_hourglass);
    snapshot_load_canvas("TMP/paintHD_zoom.bin");

    zoom_area_pixels_save();
    zoom_draw_view();

    /* init mouse */
    mouse_pos_x = (int16_t)(GFX_CANVAS_WIDTH / 2u);
    mouse_pos_y = (int16_t)(GFX_CANVAS_HEIGHT / 2u);
    xram0_struct_set(POINTER_STRUCT, vga_mode3_config_t, x_pos_px, mouse_pos_x - 7);
    xram0_struct_set(POINTER_STRUCT, vga_mode3_config_t, y_pos_px, mouse_pos_y - 7);
    mouse_init();
    draw_pointer(POINTER_arrow);

    prev_buttons = 0;
    prev_enter   = 0;
    prev_escape  = 0;
    zoom_paint_armed = 0;

    while (1)
    {
        /* ── keyboard ── */
        if (key_pressed(HID_ENTER))
        {
            if (!prev_enter)
            {
                prev_enter = 1u;
                /* apply edits, save modified canvas to zoom snapshot */
                draw_pointer(POINTER_hourglass);
                zoom_apply_changes();
                snapshot_save_canvas("TMP/paintHD_zoom.bin");
                VIA.ier = 0x40;
                ria_execl("paintHDeditor.rp6502", bmp_name,
                          "--from-zoom", undo_str, redo_str, NULL);
                return 0;
            }
        }
        else
        {
            prev_enter = 0;
        }

        if (key_pressed(HID_ESCAPE))
        {
            if (!prev_escape)
            {
                prev_escape = 1u;
                /* restore canvas from backup (removes zoom view artifacts) */
                snapshot_load_canvas("TMP/paintHD_zoom.bin");
                VIA.ier = 0x40;
                ria_execl("paintHDeditor.rp6502", bmp_name,
                          "--from-zoom", undo_str, redo_str, NULL);
                return 0;
            }
        }
        else
        {
            prev_escape = 0;
        }

        /* ── mouse buttons ── */
        RIA.addr0 = MOUSE_INPUT_BUTTONS;
        RIA.step0 = 0;
        buttons = RIA.rw0;

        x = (int)mouse_pos_x;
        y = (int)mouse_pos_y;

        if ((buttons & 1u) && !(prev_buttons & 1u))
        {
            /* LMB press — toggle pixel in 1bpp ZOOM_BUF */
            int bx = x - (int)ZOOM_VIEW_X0;
            int by = y - (int)ZOOM_VIEW_Y0;
            if (bx >= 0 && bx < (int)ZOOM_VIEW_W && by >= 0 && by < (int)ZOOM_VIEW_H)
            {
                uint8_t cur, msk;
                int sx = bx / ZOOM_DOT;
                int sy = by / ZOOM_DOT;
                RIA.step0 = 0;
                RIA.addr0 = ZOOM_BUF_ADDR + (unsigned)sy * (ZOOM_AREA_W / 8u) + (unsigned)sx / 8u;
                cur = RIA.rw0;
                msk = (uint8_t)(0x80u >> (sx & 7));
                zoom_paint_value = (cur & msk) ? 0u : 1u;   /* toggled value */
                if (zoom_paint_value) cur |= msk; else cur &= ~msk;
                RIA.rw0 = cur;
                zoom_redraw_block(sx, sy);
                zoom_paint_armed = 1u;
                zoom_paint_last_sx = sx;
                zoom_paint_last_sy = sy;
            }
        }

        if (!(buttons & 1u))
            zoom_paint_armed = 0;

        /* ── mouse move / drag ── */
        if (zoom_paint_armed)
        {
            int bx = x - (int)ZOOM_VIEW_X0;
            int by = y - (int)ZOOM_VIEW_Y0;
            if (bx >= 0 && bx < (int)ZOOM_VIEW_W && by >= 0 && by < (int)ZOOM_VIEW_H)
            {
                int sx = bx / ZOOM_DOT;
                int sy = by / ZOOM_DOT;
                if (sx != zoom_paint_last_sx || sy != zoom_paint_last_sy)
                {
                    uint8_t cur, msk;
                    RIA.step0 = 0;
                    RIA.addr0 = ZOOM_BUF_ADDR + (unsigned)sy * (ZOOM_AREA_W / 8u) + (unsigned)sx / 8u;
                    cur = RIA.rw0;
                    msk = (uint8_t)(0x80u >> (sx & 7));
                    if (zoom_paint_value) cur |= msk; else cur &= ~msk;
                    RIA.rw0 = cur;
                    zoom_redraw_block(sx, sy);
                    zoom_paint_last_sx = sx;
                    zoom_paint_last_sy = sy;
                }
            }
        }

        prev_buttons = buttons;
    }
}

/*
static void raw_toggle_pixel(int x, int y)
{
    uint8_t mask;
    if (x < 0 || x >= (int)GFX_CANVAS_WIDTH || y < 0 || y >= (int)GFX_CANVAS_HEIGHT)
        return;
    RIA.step0 = 0;
    RIA.addr0 = CANVAS_DATA + (unsigned)y * CANVAS_STRIDE + (unsigned)x / 8u;
    mask = (uint8_t)(0x80u >> (x & 7));
    RIA.rw0 = RIA.rw0 ^ mask;
}
*/