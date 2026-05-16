/*
 * PaintHD
 * Copyright (c) 2026 WojciechGw
 * based on paint.c example
 * Copyright (c) 2025 Rumbledethumps
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "paintHD.h"

#define RELEASE

//#define WITHMALLOC

#ifdef WITHMALLOC
    void *__fastcall__ argv_mem(size_t size) { return malloc(size); }
#else
    void *__fastcall__ argv_mem(size_t size) {
        static uint8_t buf[512];
        if (size > sizeof(buf)) return NULL;
        return buf;
    }
#endif

static uint8_t left_color;
static uint8_t right_color;
static uint8_t active_color;
static uint8_t brush_size;
static uint8_t brush_shape;
static uint8_t active_tool;
static bool is_drawing;
static bool is_dragging;
static bool has_line_anchor;
static bool ctrl_line_session_active;
static bool canvas_dirty;
static bool drawing_session_active;
static bool vert_brush_flipped;
static bool diag_brush_flipped;
static bool mirror_vertical;
static bool selection_active;
static bool selection_dragging;
static bool selection_overlay_visible;
static bool primitive_dragging;
static bool primitive_overlay_visible;
static bool paste_preview_active;
static bool paste_preview_visible;
static bool paste_transparent;
static bool crosshair_active;
static bool crosshair_visible;
static int crosshair_x;
static int crosshair_y;
static bool line_anchor_marker_visible;
static int line_anchor_marker_x;
static int line_anchor_marker_y;
static bool zoom_area_active;
static bool zoom_area_visible;
static bool zoom_view_active;
static int zoom_area_x;
static int zoom_area_y;
static int zoom_area_marker_x;
static int zoom_area_marker_y;
static uint8_t drawing_button;
static uint8_t left_draw_armed;
static uint8_t right_draw_armed;
static uint8_t zoom_paint_armed;
static uint8_t zoom_paint_value;
static int zoom_paint_last_sx;
static int zoom_paint_last_sy;
static uint8_t circle_cached_size;
static uint8_t circle_row_left[BRUSH_MAX];
static uint8_t circle_row_right[BRUSH_MAX];
static int picker_x, picker_y;
// static int zoom_saved_picker_x, zoom_saved_picker_y;
static int drag_x, drag_y;
static int line_anchor_x, line_anchor_y;
static int line_x, line_y;
static int selection_x1, selection_y1;
static int selection_x2, selection_y2;
static int selection_anchor_x, selection_anchor_y;
static int primitive_x1, primitive_y1;
static int primitive_x2, primitive_y2;
static int primitive_anchor_x, primitive_anchor_y;
static int paste_x, paste_y;
static const char *save_bmp_path;
static char picker_save_hover[32];
static char picker_status[24];
static const char *picker_hover_status;
static clock_t picker_status_deadline;
static clock_t size_click_deadline;
static clock_t mirror_click_deadline;
static clock_t ctrl_hover_deadline;
static uint8_t startup_splash_pending;
static uint8_t help_pending;
static uint8_t picker_collapsed;
static int g_argc;
static char **g_argv;
static uint8_t size_click_pending;
static uint8_t mirror_click_pending;
static uint8_t clipboard_valid;
static uint8_t clipboard_preview_valid;
static uint8_t undo_count;
static uint8_t redo_count;
static uint8_t undo_enabled = 1u;
static uint16_t clipboard_width;
static uint16_t clipboard_height;
static uint16_t clipboard_stride;
static uint16_t clipboard_preview_top;
static uint16_t clipboard_preview_bottom;
static uint16_t clipboard_preview_left_x;
static uint16_t clipboard_preview_right_x;
static uint16_t primitive_ellipse_left[GFX_CANVAS_HEIGHT];
static uint16_t primitive_ellipse_right[GFX_CANVAS_HEIGHT];
static uint8_t clipboard_row[CANVAS_STRIDE];
static uint8_t clipboard_header[10];
static uint8_t undo_dirty_stack[16];
static uint8_t redo_dirty_stack[16];
static uint8_t primitive_ellipse_ready;

static const char clipboard_path[] = "TMP/painthd_clip.bin";
static const char current_snapshot_path[] = "TMP/paintHD_c.bin";
static uint8_t current_snapshot_valid;
static uint8_t operation_cancelled;
static uint8_t busy_mode;
static uint8_t current_cursor;

static uint8_t mouse_stack[8];
static int16_t mouse_pos_x;
static int16_t mouse_pos_y;

static uint16_t prng_state = 1;

static uint8_t prng_next(void)
{
    prng_state = prng_state * 25173u + 13849u;
    return (uint8_t)(prng_state >> 8);
}

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

    RIA.addr0 = MOUSE_INPUT + 1;
    rw = RIA.rw0;
    raw_x += (int8_t)(rw - prev_x);
    prev_x = rw;
    if (raw_x < 0)
        raw_x = 0;
    if (raw_x > (GFX_CANVAS_WIDTH - 1) * MOUSE_DIV)
        raw_x = (GFX_CANVAS_WIDTH - 1) * MOUSE_DIV;

    RIA.addr0 = MOUSE_INPUT + 2;
    rw = RIA.rw0;
    raw_y += (int8_t)(rw - prev_y);
    prev_y = rw;
    if (raw_y < 0)
        raw_y = 0;
    if (raw_y > (GFX_CANVAS_HEIGHT - 1) * MOUSE_DIV)
        raw_y = (GFX_CANVAS_HEIGHT - 1) * MOUSE_DIV;

    mouse_pos_x = raw_x / MOUSE_DIV;
    mouse_pos_y = raw_y / MOUSE_DIV;
    xram0_struct_set(POINTER_STRUCT, vga_mode3_config_t, x_pos_px, mouse_pos_x - 7);
    xram0_struct_set(POINTER_STRUCT, vga_mode3_config_t, y_pos_px, mouse_pos_y - 7);

    RIA.addr0 = save_addr0;
    RIA.step0 = save_step0;
    return IRQ_HANDLED;
}

static void mouse_init(void)
{
    // 125Hz from the VIA
    // unsigned timer_val = (ria_attr_get(RIA_ATTR_PHI2_KHZ) * 8u) - 2u;
    // 200Hz from the VIA
    unsigned timer_val = (ria_attr_get(RIA_ATTR_PHI2_KHZ) * 5u) - 2u;
    // 400Hz from the VIA
    // unsigned timer_val = (ria_attr_get(RIA_ATTR_PHI2_KHZ) * 5u) / 2u - 2u;

    VIA.t1l_lo = timer_val & 0xFF;
    VIA.t1l_hi = timer_val >> 8;
    VIA.t1_lo = timer_val & 0xFF;
    VIA.t1_hi = timer_val >> 8;
    VIA.acr = 0x40;
    VIA.ier = 0xC0;
    xreg_ria_mouse(MOUSE_INPUT);
    set_irq(mouse_irq_fn, &mouse_stack, sizeof(mouse_stack));
}

static void busy_begin(void)
{
    busy_mode = 1u;
    strncpy(picker_status, "PLEASE WAIT", sizeof(picker_status) - 1u);
    picker_status[sizeof(picker_status) - 1u] = '\0';
    picker_status_deadline = 0;
    draw_picker_status();
    draw_pointer(POINTER_hourglass);
}

static void busy_end(void)
{
    busy_mode = 0u;
    picker_status[0] = '\0';
    picker_status_deadline = 0;
    draw_picker_status();
    draw_pointer(current_cursor);
}

static void fill_canvas(uint8_t color, uint8_t pattern, uint8_t linestep, uint8_t overlay)
{
    unsigned row;
    unsigned col;
    unsigned addr;
    uint8_t base_fill;
    uint8_t set_mask;
    uint8_t clr_mask;
    uint8_t fill;

    base_fill = color ? 0xFF : 0x00;
    if (overlay)
    {
        /* set_mask: bits in pattern → forced to color; others unchanged */
        set_mask = pattern & base_fill;
        clr_mask = pattern & ~base_fill;
        for (row = 0; row < GFX_CANVAS_HEIGHT; row++)
        {
            if ((row & 0x1Fu) == 0u && operation_cancel_requested())
                break;
            if (linestep > 0u && (row % linestep) == 0u)
            {
                addr = CANVAS_DATA + row * CANVAS_STRIDE;
                for (col = 0; col < CANVAS_STRIDE; col++)
                {
                    RIA.step0 = 0;
                    RIA.addr0 = addr + col;
                    RIA.rw0 = (RIA.rw0 | set_mask) & ~clr_mask;
                }
            }
        }
    }
    else
    {
        for (row = 0; row < GFX_CANVAS_HEIGHT; row++)
        {
            if ((row & 0x1Fu) == 0u && operation_cancel_requested())
                break;
            fill = (linestep > 0u && (row % linestep) == 0u)
                   ? (base_fill ^ pattern) : base_fill;
            addr = CANVAS_DATA + row * CANVAS_STRIDE;
            RIA.step0 = 1;
            for (col = 0; col < CANVAS_STRIDE / 8u; col++, addr += 8u)
            {
                RIA.addr0 = addr;
                RIA.rw0 = fill;
                RIA.rw0 = fill;
                RIA.rw0 = fill;
                RIA.rw0 = fill;
                RIA.rw0 = fill;
                RIA.rw0 = fill;
                RIA.rw0 = fill;
                RIA.rw0 = fill;
            }
        }
    }
}

static void operation_cancel_begin(void)
{
    operation_cancelled = 0u;
}

static uint8_t operation_cancel_requested(void)
{
    if (key_pressed(HID_ESCAPE))
    {
        operation_cancelled = 1u;
        return 1u;
    }
    return 0u;
}

static uint8_t operation_was_cancelled(void)
{
    return operation_cancelled;
}

static void clear_canvas_random_blocks8(void)
{
    unsigned cleared;
    unsigned tile_index;
    unsigned tile_x;
    unsigned tile_y;
    unsigned base;
    uint16_t state;
    uint16_t lsb;
    uint8_t row;

    state = (uint16_t)prng_next();
    state |= (uint16_t)((uint16_t)prng_next() << 8);
    state &= 0x1FFFu;
    if (state == 0u)
        state = 1u;

    RIA.step0 = 0;
    for (cleared = 0u; cleared < ((unsigned)GFX_CANVAS_WIDTH / 8u) * ((unsigned)GFX_CANVAS_HEIGHT / 8u); )
    {
        if ((cleared & 0x3Fu) == 0u && operation_cancel_requested())
            break;
        tile_index = (unsigned)(state - 1u);
        if (tile_index < ((unsigned)GFX_CANVAS_WIDTH / 8u) * ((unsigned)GFX_CANVAS_HEIGHT / 8u))
        {
            tile_x = tile_index % (GFX_CANVAS_WIDTH / 8u);
            tile_y = tile_index / (GFX_CANVAS_WIDTH / 8u);
            base = CANVAS_DATA + tile_y * (unsigned)(CANVAS_STRIDE * 8u) + tile_x;

            for (row = 0; row < 8u; row++)
            {
                RIA.addr0 = base + (unsigned)row * CANVAS_STRIDE;
                RIA.rw0 = 0u;
            }
            cleared++;
        }

        lsb = (uint16_t)(state & 1u);
        state >>= 1;
        if (lsb != 0u)
            state ^= 0x1C80u;
    }
}

static uint8_t snapshot_save_canvas(const char *path)
{
    uint8_t selection_was_visible;
    uint8_t primitive_was_visible;
    uint8_t paste_was_visible;
    uint16_t addr;
    uint16_t remaining;
    unsigned chunk;
    int fd;

    selection_was_visible = selection_overlay_visible ? 1u : 0u;
    primitive_was_visible = primitive_overlay_visible ? 1u : 0u;
    paste_was_visible = paste_preview_visible ? 1u : 0u;
    line_anchor_hide_marker();
    crosshair_hide();
    paste_preview_hide();
    primitive_hide_overlay();
    selection_hide_overlay();

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
    {
        if (selection_was_visible)
            selection_show_overlay();
        if (primitive_was_visible)
            primitive_show_overlay();
        if (paste_was_visible)
            paste_preview_show();
        crosshair_show();
        if (has_line_anchor)
            line_anchor_show_marker();
        return 0;
    }

    addr = CANVAS_DATA;
    remaining = (uint16_t)(GFX_CANVAS_HEIGHT * CANVAS_STRIDE);
    while (remaining > 0)
    {
        if (operation_cancel_requested())
        {
            close(fd);
            if (selection_was_visible)
                selection_show_overlay();
            if (primitive_was_visible)
                primitive_show_overlay();
            if (paste_was_visible)
                paste_preview_show();
            crosshair_show();
            if (has_line_anchor)
                line_anchor_show_marker();
            return 0;
        }
        chunk = (remaining > 0x7FFFu) ? 0x7FFFu : (unsigned)remaining;
        if (write_xram(addr, chunk, fd) < 0)
        {
            close(fd);
            if (selection_was_visible)
                selection_show_overlay();
            if (primitive_was_visible)
                primitive_show_overlay();
            if (paste_was_visible)
                paste_preview_show();
            crosshair_show();
            if (has_line_anchor)
                line_anchor_show_marker();
            return 0;
        }
        addr += (uint16_t)chunk;
        remaining -= (uint16_t)chunk;
    }

    close(fd);
    if (selection_was_visible)
        selection_show_overlay();
    if (primitive_was_visible)
        primitive_show_overlay();
    if (paste_was_visible)
        paste_preview_show();
    crosshair_show();
    if (has_line_anchor)
        line_anchor_show_marker();
    return 1u;
}

static uint8_t snapshot_load_canvas(const char *path)
{
    uint8_t selection_was_visible;
    uint8_t primitive_was_visible;
    uint8_t paste_was_visible;
    uint16_t addr;
    uint16_t remaining;
    unsigned chunk;
    int fd;

    selection_was_visible = selection_overlay_visible ? 1u : 0u;
    primitive_was_visible = primitive_overlay_visible ? 1u : 0u;
    paste_was_visible = paste_preview_visible ? 1u : 0u;
    line_anchor_hide_marker();
    crosshair_hide();
    paste_preview_hide();
    primitive_hide_overlay();
    selection_hide_overlay();

    fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        if (selection_was_visible)
            selection_show_overlay();
        if (primitive_was_visible)
            primitive_show_overlay();
        if (paste_was_visible)
            paste_preview_show();
        crosshair_show();
        if (has_line_anchor)
            line_anchor_show_marker();
        return 0;
    }

    addr = CANVAS_DATA;
    remaining = (uint16_t)(GFX_CANVAS_HEIGHT * CANVAS_STRIDE);
    while (remaining > 0)
    {
        if (operation_cancel_requested())
        {
            close(fd);
            if (selection_was_visible)
                selection_show_overlay();
            if (primitive_was_visible)
                primitive_show_overlay();
            if (paste_was_visible)
                paste_preview_show();
            crosshair_show();
            if (has_line_anchor)
                line_anchor_show_marker();
            return 0;
        }
        chunk = (remaining > 0x7FFFu) ? 0x7FFFu : (unsigned)remaining;
        if (read_xram(addr, chunk, fd) != (int)chunk)
        {
            close(fd);
            if (selection_was_visible)
                selection_show_overlay();
            if (primitive_was_visible)
                primitive_show_overlay();
            if (paste_was_visible)
                paste_preview_show();
            crosshair_show();
            if (has_line_anchor)
                line_anchor_show_marker();
            return 0;
        }
        addr += (uint16_t)chunk;
        remaining -= (uint16_t)chunk;
    }

    close(fd);
    if (selection_was_visible)
        selection_show_overlay();
    if (primitive_was_visible)
        primitive_show_overlay();
    if (paste_was_visible)
        paste_preview_show();
    crosshair_show();
    if (has_line_anchor)
        line_anchor_show_marker();
    return 1u;
}

static void snapshot_path(char *out, char kind, uint8_t index)
{
    static const char prefix[] = PATHS_SNAPSHOT;
    memcpy(out, prefix, sizeof(prefix) - 1u);
    out[sizeof(prefix) - 1u] = (char)kind;
    out[sizeof(prefix)] = (char)('0' + (index / 100u));
    out[sizeof(prefix) + 1u] = (char)('0' + ((index / 10u) % 10u));
    out[sizeof(prefix) + 2u] = (char)('0' + (index % 10u));
    out[sizeof(prefix) + 3u] = '.';
    out[sizeof(prefix) + 4u] = 'b';
    out[sizeof(prefix) + 5u] = 'i';
    out[sizeof(prefix) + 6u] = 'n';
    out[sizeof(prefix) + 7u] = '\0';
}

static void snapshot_stack_clear(char kind, uint8_t *count)
{
    unsigned i;
    char path[TMP_PATH_LEN];

    for (i = 0; i < *count; i++)
    {
        snapshot_path(path, kind, (uint8_t)i);
        remove(path);
    }
    *count = 0u;
}

static uint8_t snapshot_stack_push(char kind, uint8_t *dirty_stack, uint8_t *count, uint8_t dirty_value)
{
    unsigned i;
    unsigned start;
    char from_path[TMP_PATH_LEN];
    char to_path[TMP_PATH_LEN];

    start = (*count < UNDO_LEVELS) ? *count : (UNDO_LEVELS - 1u);
    for (i = start; i > 0u; i--)
    {
        snapshot_path(to_path, kind, (uint8_t)i);
        snapshot_path(from_path, kind, (uint8_t)(i - 1u));
        remove(to_path);
        rename(from_path, to_path);
        dirty_stack[i] = dirty_stack[i - 1u];
    }

    snapshot_path(to_path, kind, 0u);
    if (!snapshot_save_canvas(to_path))
        return 0u;

    dirty_stack[0] = dirty_value;
    if (*count < UNDO_LEVELS)
        (*count)++;
    return 1u;
}

static uint8_t snapshot_stack_pop(char kind, uint8_t *dirty_stack, uint8_t *count, uint8_t *dirty_value)
{
    unsigned i;
    char from_path[TMP_PATH_LEN];
    char to_path[TMP_PATH_LEN];

    if (*count == 0u)
        return 0u;
    snapshot_path(from_path, kind, 0u);
    if (!snapshot_load_canvas(from_path))
        return 0u;

    *dirty_value = dirty_stack[0];
    for (i = 0; i + 1u < *count; i++)
    {
        snapshot_path(to_path, kind, (uint8_t)i);
        snapshot_path(from_path, kind, (uint8_t)(i + 1u));
        remove(to_path);
        rename(from_path, to_path);
        dirty_stack[i] = dirty_stack[i + 1u];
    }
    snapshot_path(to_path, kind, (uint8_t)(*count - 1u));
    remove(to_path);
    (*count)--;
    return 1u;
}

static uint8_t snapshot_stage_current(char kind, uint8_t *dirty_stack, uint8_t *count, uint8_t dirty_value)
{
    unsigned i;
    unsigned start;
    char from_path[TMP_PATH_LEN];
    char to_path[TMP_PATH_LEN];

    if (!current_snapshot_valid)
        return snapshot_stack_push(kind, dirty_stack, count, dirty_value);

    start = (*count < UNDO_LEVELS) ? *count : (UNDO_LEVELS - 1u);
    for (i = start; i > 0u; i--)
    {
        snapshot_path(to_path, kind, (uint8_t)i);
        snapshot_path(from_path, kind, (uint8_t)(i - 1u));
        remove(to_path);
        rename(from_path, to_path);
        dirty_stack[i] = dirty_stack[i - 1u];
    }

    snapshot_path(to_path, kind, 0u);
    remove(to_path);
    if (rename(current_snapshot_path, to_path) != 0)
    {
        current_snapshot_valid = 0u;
        return snapshot_stack_push(kind, dirty_stack, count, dirty_value);
    }

    dirty_stack[0] = dirty_value;
    if (*count < UNDO_LEVELS)
        (*count)++;
    current_snapshot_valid = 0u;
    return 1u;
}

static uint8_t snapshot_refresh_current(void)
{
    if (!undo_enabled)
        return 1u;
    if (!snapshot_save_canvas(current_snapshot_path))
    {
        current_snapshot_valid = 0u;
        return 0u;
    }
    current_snapshot_valid = 1u;
    return 1u;
}


static uint8_t prepare_undo_step(void)
{
    if (!undo_enabled)
        return 1u;
    operation_cancel_begin();
    if (redo_count != 0u)
        snapshot_stack_clear('r', &redo_count);
    if (!snapshot_stage_current('u', undo_dirty_stack, &undo_count,
                                canvas_dirty ? 1u : 0u))
    {
        if (operation_was_cancelled())
            set_picker_status("cancelled");
        else
            set_picker_status("undo error");
        return 0;
    }
    return 1u;
}

static void perform_undo(void)
{
    uint8_t dirty_value;

    if (!undo_enabled)
    {
        set_picker_status("undo disabled");
        return;
    }
    busy_begin();
    operation_cancel_begin();
    if (undo_count == 0u)
    {
        busy_end();
        set_picker_status("no undo");
        return;
    }
    if (!snapshot_stage_current('r', redo_dirty_stack, &redo_count,
                                canvas_dirty ? 1u : 0u))
    {
        busy_end();
        if (operation_was_cancelled())
            set_picker_status("cancelled");
        else
            set_picker_status("redo error");
        return;
    }
    if (!snapshot_stack_pop('u', undo_dirty_stack, &undo_count, &dirty_value))
    {
        busy_end();
        if (operation_was_cancelled())
            set_picker_status("cancelled");
        else
            set_picker_status("undo error");
        return;
    }

    set_canvas_dirty(dirty_value);
    if (!snapshot_refresh_current() && operation_was_cancelled())
    {
        busy_end();
        set_picker_status("cancelled");
    }
    else
    {
        busy_end();
        set_picker_status("undo");
    }
}

static void perform_redo(void)
{
    uint8_t dirty_value;

    if (!undo_enabled)
    {
        set_picker_status("undo disabled");
        return;
    }
    busy_begin();
    operation_cancel_begin();
    if (redo_count == 0u)
    {
        busy_end();
        set_picker_status("no redo");
        return;
    }
    if (!snapshot_stage_current('u', undo_dirty_stack, &undo_count,
                                canvas_dirty ? 1u : 0u))
    {
        busy_end();
        if (operation_was_cancelled())
            set_picker_status("cancelled");
        else
            set_picker_status("undo error");
        return;
    }
    if (!snapshot_stack_pop('r', redo_dirty_stack, &redo_count, &dirty_value))
    {
        busy_end();
        if (operation_was_cancelled())
            set_picker_status("cancelled");
        else
            set_picker_status("redo error");
        return;
    }

    set_canvas_dirty(dirty_value);
    if (!snapshot_refresh_current() && operation_was_cancelled())
    {
        busy_end();
        set_picker_status("cancelled");
    }
    else
    {
        busy_end();
        set_picker_status("redo");
    }
}

static void normalize_rect(int *x1, int *y1, int *x2, int *y2)
{
    int t;

    if (*x1 > *x2)
    {
        t = *x1;
        *x1 = *x2;
        *x2 = t;
    }
    if (*y1 > *y2)
    {
        t = *y1;
        *y1 = *y2;
        *y2 = t;
    }
}

static uint8_t selection_point_allowed(int x, int y)
{
    if (!selection_active)
        return 1;
    return x >= selection_x1 && x <= selection_x2 &&
           y >= selection_y1 && y <= selection_y2;
}

static uint8_t selection_clip_rect(int *x1, int *y1, int *x2, int *y2)
{
    if (selection_active)
    {
        if (*x1 < selection_x1) *x1 = selection_x1;
        if (*y1 < selection_y1) *y1 = selection_y1;
        if (*x2 > selection_x2) *x2 = selection_x2;
        if (*y2 > selection_y2) *y2 = selection_y2;
    }
    return *x1 <= *x2 && *y1 <= *y2;
}

static void raw_set_pixel(int x, int y, uint8_t color)
{
    uint8_t mask;

    if (x < 0 || x >= (int)GFX_CANVAS_WIDTH || y < 0 || y >= (int)GFX_CANVAS_HEIGHT)
        return;
    RIA.step0 = 0;
    RIA.addr0 = (unsigned)y * CANVAS_STRIDE + (unsigned)x / 8;
    mask = (uint8_t)(0x80u >> (x & 7));
    if (color) RIA.rw0 = RIA.rw0 | mask;
    else       RIA.rw0 = RIA.rw0 & (uint8_t)~mask;
}

static uint8_t raw_get_pixel(int x, int y)
{
    uint8_t mask;

    if (x < 0 || x >= (int)GFX_CANVAS_WIDTH || y < 0 || y >= (int)GFX_CANVAS_HEIGHT)
        return 0;
    RIA.step0 = 0;
    RIA.addr0 = (unsigned)y * CANVAS_STRIDE + (unsigned)x / 8;
    mask = (uint8_t)(0x80u >> (x & 7));
    return (RIA.rw0 & mask) ? 1u : 0u;
}

static void raw_toggle_pixel(int x, int y)
{
    uint8_t mask;

    if (x < 0 || x >= (int)GFX_CANVAS_WIDTH || y < 0 || y >= (int)GFX_CANVAS_HEIGHT)
        return;
    RIA.step0 = 0;
    RIA.addr0 = (unsigned)y * CANVAS_STRIDE + (unsigned)x / 8;
    mask = (uint8_t)(0x80u >> (x & 7));
    RIA.rw0 = RIA.rw0 ^ mask;
}

static void toggle_pixel_byte_range(int x1, int x2, int y, uint8_t use_checker)
{
    int bx, bx1, bx2;
    uint8_t lmask, rmask, range_mask, toggle_mask;
    unsigned row_base;

    if (x1 > x2) return;
    if (x1 < 0) x1 = 0;
    if (x2 >= (int)GFX_CANVAS_WIDTH) x2 = (int)GFX_CANVAS_WIDTH - 1;
    if (x1 > x2) return;

    bx1 = x1 / 8;
    bx2 = x2 / 8;
    row_base = (unsigned)y * CANVAS_STRIDE;

    for (bx = bx1; bx <= bx2; bx++)
    {
        uint8_t checker;
        lmask = (bx == bx1) ? (uint8_t)(0xFFu >> (x1 & 7)) : 0xFFu;
        rmask = (bx == bx2) ? (uint8_t)(0xFFu << (7 - (x2 & 7))) : 0xFFu;
        range_mask = (uint8_t)(lmask & rmask);
        if (use_checker)
        {
            checker = (((unsigned)(bx * 8) + (unsigned)y) & 1u) ? 0x55u : 0xAAu;
            toggle_mask = (uint8_t)(range_mask & checker);
        }
        else
        {
            toggle_mask = range_mask;
        }
        if (toggle_mask == 0u) continue;
        RIA.step0 = 0;
        RIA.addr0 = row_base + (unsigned)bx;
        RIA.rw0 = RIA.rw0 ^ toggle_mask;
    }
}

static void selection_toggle_overlay(void)
{
    int y;
    uint8_t use_checker;

    if ((!selection_active && !selection_dragging) ||
        selection_x1 > selection_x2 || selection_y1 > selection_y2)
        return;

    use_checker = selection_dragging ? 0u : 1u;

    toggle_pixel_byte_range(selection_x1, selection_x2, selection_y1, use_checker);
    if (selection_y2 != selection_y1)
        toggle_pixel_byte_range(selection_x1, selection_x2, selection_y2, use_checker);
    for (y = selection_y1 + 1; y < selection_y2; y++)
    {
        toggle_pixel_byte_range(selection_x1, selection_x1, y, use_checker);
        if (selection_x2 != selection_x1)
            toggle_pixel_byte_range(selection_x2, selection_x2, y, use_checker);
    }
}

static void selection_hide_overlay(void)
{
    if (!selection_overlay_visible)
        return;
    selection_toggle_overlay();
    selection_overlay_visible = false;
}

static void selection_show_overlay(void)
{
    if (selection_overlay_visible)
        return;
    if ((!selection_active && !selection_dragging) ||
        selection_x1 > selection_x2 || selection_y1 > selection_y2)
        return;
    selection_toggle_overlay();
    selection_overlay_visible = true;
}

static uint8_t primitive_mode_active(void)
{
    return (active_tool == TOOL_RECT || active_tool == TOOL_ELLIPSE || primitive_dragging) ? 1u : 0u;
}

static uint8_t primitive_tools_enabled(void)
{
    return brush_shape != SHAPE_FILL;
}

static uint8_t brush_shape_enabled(uint8_t shape)
{
    if (shape == SHAPE_FILL &&
        (active_tool == TOOL_RECT || active_tool == TOOL_ELLIPSE))
        return 0u;
    return 1u;
}

static void primitive_apply_constraint(int anchor_x, int anchor_y, int *x, int *y)
{
    int dx;
    int dy;
    int ax;
    int ay;
    int size;

    dx = *x - anchor_x;
    dy = *y - anchor_y;
    ax = dx < 0 ? -dx : dx;
    ay = dy < 0 ? -dy : dy;
    size = ax > ay ? ax : ay;
    if (dx < 0)
        *x = anchor_x - size;
    else
        *x = anchor_x + size;
    if (dy < 0)
        *y = anchor_y - size;
    else
        *y = anchor_y + size;
}

static void primitive_update_rect(int x1, int y1, int x2, int y2)
{
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 < 0) x2 = 0;
    if (y2 < 0) y2 = 0;
    if (x1 >= (int)GFX_CANVAS_WIDTH)  x1 = (int)GFX_CANVAS_WIDTH - 1;
    if (x2 >= (int)GFX_CANVAS_WIDTH)  x2 = (int)GFX_CANVAS_WIDTH - 1;
    if (y1 >= (int)GFX_CANVAS_HEIGHT) y1 = (int)GFX_CANVAS_HEIGHT - 1;
    if (y2 >= (int)GFX_CANVAS_HEIGHT) y2 = (int)GFX_CANVAS_HEIGHT - 1;
    normalize_rect(&x1, &y1, &x2, &y2);
    primitive_x1 = x1;
    primitive_y1 = y1;
    primitive_x2 = x2;
    primitive_y2 = y2;
    primitive_ellipse_ready = 0u;
}

static void primitive_store_ellipse_point(int x, int y)
{
    unsigned row;

    if (y < primitive_y1 || y > primitive_y2)
        return;
    row = (unsigned)(y - primitive_y1);
    if (primitive_ellipse_left[row] == 0xFFFFu || (unsigned)x < primitive_ellipse_left[row])
        primitive_ellipse_left[row] = (uint16_t)x;
    if (primitive_ellipse_right[row] == 0xFFFFu || (unsigned)x > primitive_ellipse_right[row])
        primitive_ellipse_right[row] = (uint16_t)x;
}

static void primitive_build_ellipse_rows(void)
{
    int x0;
    int y0;
    int x1;
    int y1;
    int h;
    int row;
    int t;
    long a;
    int b;
    long b1;
    long dx;
    long dy;
    long err;
    long e2;

    h = primitive_y2 - primitive_y1 + 1;
    if (h <= 0)
    {
        primitive_ellipse_ready = 0u;
        return;
    }
    for (row = 0; row < h; row++)
    {
        if ((row & 0x1Fu) == 0 && operation_cancel_requested())
        {
            primitive_ellipse_ready = 0u;
            return;
        }
        primitive_ellipse_left[row] = 0xFFFFu;
        primitive_ellipse_right[row] = 0xFFFFu;
    }

    x0 = primitive_x1;
    y0 = primitive_y1;
    x1 = primitive_x2;
    y1 = primitive_y2;
    a = x1 - x0;
    b = y1 - y0;
    if (a <= 0 || b <= 0)
    {
        primitive_store_ellipse_point(x0, y0);
        primitive_store_ellipse_point(x1, y0);
        primitive_store_ellipse_point(x0, y1);
        primitive_store_ellipse_point(x1, y1);
        primitive_ellipse_ready = 1u;
        return;
    }

    b1 = b & 1;
    dx = 4L * (1L - a) * b * b;
    dy = 4L * (b1 + 1L) * a * a;
    err = dx + dy + b1 * a * a;

    if (x0 > x1)
    {
        t = x0;
        x0 = x1;
        x1 = t;
    }
    if (y0 > y1)
    {
        t = y0;
        y0 = y1;
        y1 = t;
    }
    y0 += (int)((b + 1L) / 2L);
    y1 = y0 - (int)b1;
    a = 8L * a * a;
    b1 = 8L * b * b;

    do
    {
        if (operation_cancel_requested())
        {
            primitive_ellipse_ready = 0u;
            return;
        }
        primitive_store_ellipse_point(x1, y0);
        primitive_store_ellipse_point(x0, y0);
        primitive_store_ellipse_point(x0, y1);
        primitive_store_ellipse_point(x1, y1);
        e2 = 2L * err;
        if (e2 <= dy)
        {
            y0++;
            y1--;
            err += dy += a;
        }
        if (e2 >= dx || 2L * err > dy)
        {
            x0++;
            x1--;
            err += dx += b1;
        }
    } while (x0 <= x1);

    while (y0 - y1 <= b)
    {
        if (operation_cancel_requested())
        {
            primitive_ellipse_ready = 0u;
            return;
        }
        primitive_store_ellipse_point(x0 - 1, y0);
        primitive_store_ellipse_point(x1 + 1, y0);
        primitive_store_ellipse_point(x0 - 1, y1);
        primitive_store_ellipse_point(x1 + 1, y1);
        y0++;
        y1--;
    }

    primitive_ellipse_ready = 1u;
}

static void primitive_toggle_point_overlay(void)
{
    int x;
    int y;

    x = primitive_x1;
    y = primitive_y1;
    if (selection_point_allowed(x, y))
        raw_toggle_pixel(x, y);
    if (selection_point_allowed(x - 1, y))
        raw_toggle_pixel(x - 1, y);
    if (selection_point_allowed(x + 1, y))
        raw_toggle_pixel(x + 1, y);
    if (selection_point_allowed(x, y - 1))
        raw_toggle_pixel(x, y - 1);
    if (selection_point_allowed(x, y + 1))
        raw_toggle_pixel(x, y + 1);
}

static void primitive_toggle_rect_overlay(void)
{
    int y;
    if (!selection_active)
    {
        toggle_pixel_byte_range(primitive_x1, primitive_x2, primitive_y1, 0u);
        if (primitive_y2 != primitive_y1)
            toggle_pixel_byte_range(primitive_x1, primitive_x2, primitive_y2, 0u);
        for (y = primitive_y1 + 1; y < primitive_y2; y++)
        {
            toggle_pixel_byte_range(primitive_x1, primitive_x1, y, 0u);
            if (primitive_x2 != primitive_x1)
                toggle_pixel_byte_range(primitive_x2, primitive_x2, y, 0u);
        }
    }
    else
    {
        int x;
        for (x = primitive_x1; x <= primitive_x2; x++)
        {
            if (selection_point_allowed(x, primitive_y1))
                raw_toggle_pixel(x, primitive_y1);
            if (primitive_y2 != primitive_y1 && selection_point_allowed(x, primitive_y2))
                raw_toggle_pixel(x, primitive_y2);
        }
        for (y = primitive_y1 + 1; y < primitive_y2; y++)
        {
            if (selection_point_allowed(primitive_x1, y))
                raw_toggle_pixel(primitive_x1, y);
            if (primitive_x2 != primitive_x1 && selection_point_allowed(primitive_x2, y))
                raw_toggle_pixel(primitive_x2, y);
        }
    }
}

static void primitive_toggle_ellipse_overlay(void)
{
    int y;
    int row;
    int left;
    int right;
    int first_row;
    int last_row;

    if (!primitive_ellipse_ready)
        primitive_build_ellipse_rows();
    if (!primitive_ellipse_ready)
        return;

    first_row = -1;
    last_row = -1;
    for (row = 0; row <= primitive_y2 - primitive_y1; row++)
    {
        if (primitive_ellipse_left[row] != 0xFFFFu)
        {
            if (first_row < 0)
                first_row = row;
            last_row = row;
        }
    }

    if (!selection_active)
    {
        for (row = 0; row <= primitive_y2 - primitive_y1; row++)
        {
            left = (int)primitive_ellipse_left[row];
            if (left == 0xFFFF) continue;
            right = (int)primitive_ellipse_right[row];
            y = primitive_y1 + row;
            if (row == first_row || row == last_row)
                toggle_pixel_byte_range(left, right, y, 0u);
            else
            {
                raw_toggle_pixel(left, y);
                if (right != left) raw_toggle_pixel(right, y);
            }
        }
    }
    else
    {
        for (row = 0; row <= primitive_y2 - primitive_y1; row++)
        {
            left = (int)primitive_ellipse_left[row];
            if (left == 0xFFFF) continue;
            right = (int)primitive_ellipse_right[row];
            y = primitive_y1 + row;
            if (row == first_row || row == last_row)
            {
                int x;
                for (x = left; x <= right; x++)
                    if (selection_point_allowed(x, y))
                        raw_toggle_pixel(x, y);
            }
            else
            {
                if (selection_point_allowed(left, y))
                    raw_toggle_pixel(left, y);
                if (right != left && selection_point_allowed(right, y))
                    raw_toggle_pixel(right, y);
            }
        }
    }
}

static void primitive_toggle_overlay(void)
{
    if (!primitive_dragging || primitive_x1 > primitive_x2 || primitive_y1 > primitive_y2)
        return;
    if (primitive_x1 == primitive_x2 && primitive_y1 == primitive_y2)
    {
        primitive_toggle_point_overlay();
        return;
    }
    if (active_tool == TOOL_ELLIPSE)
        primitive_toggle_ellipse_overlay();
    else
        primitive_toggle_rect_overlay();
}

static void primitive_hide_overlay(void)
{
    if (!primitive_overlay_visible)
        return;
    primitive_toggle_overlay();
    primitive_overlay_visible = false;
}

static void primitive_show_overlay(void)
{
    if (primitive_overlay_visible || !primitive_dragging)
        return;
    primitive_toggle_overlay();
    primitive_overlay_visible = true;
}

static void update_primitive_size_status(void)
{
    char text[24];
    char *out;
    unsigned width;
    unsigned height;
    const char *prefix;
    unsigned i;

    if (!primitive_dragging)
        return;

    width = (unsigned)(primitive_x2 - primitive_x1 + 1);
    height = (unsigned)(primitive_y2 - primitive_y1 + 1);
    prefix = (active_tool == TOOL_ELLIPSE) ? "ellipse " : "rectangle ";
    out = text;
    for (i = 0; prefix[i] != '\0'; i++)
        *out++ = prefix[i];
    out = append_coord_value(out, (int)width);
    *out++ = 'x';
    out = append_coord_value(out, (int)height);
    *out = '\0';
    set_picker_status(text);
}

static void primitive_begin_drag(uint8_t tool, uint8_t button, uint8_t color, int x, int y)
{
    (void)tool;
    primitive_hide_overlay();
    primitive_dragging = true;
    is_drawing = false;
    left_draw_armed = 0;
    right_draw_armed = 0;
    primitive_anchor_x = x;
    primitive_anchor_y = y;
    drawing_button = button;
    active_color = color;
    primitive_update_rect(x, y, x, y);
    primitive_show_overlay();
    update_primitive_size_status();
}

static void primitive_update_drag(int x, int y)
{
    if (!primitive_dragging)
        return;
    if (key_pressed(HID_LEFT_CTRL))
        primitive_apply_constraint(primitive_anchor_x, primitive_anchor_y, &x, &y);
    primitive_hide_overlay();
    primitive_update_rect(primitive_anchor_x, primitive_anchor_y, x, y);
    primitive_show_overlay();
    update_primitive_size_status();
}

static void primitive_cancel(void)
{
    primitive_hide_overlay();
    primitive_dragging = false;
    is_drawing = false;
    left_draw_armed = 0;
    right_draw_armed = 0;
    primitive_x1 = 1;
    primitive_y1 = 1;
    primitive_x2 = 0;
    primitive_y2 = 0;
    drawing_button = DRAW_BUTTON_NONE;
}

static void update_selection_size_status(void)
{
    char text[24];
    char *out;
    unsigned width;
    unsigned height;

    if (!selection_dragging)
        return;

    width = (unsigned)(selection_x2 - selection_x1 + 1);
    height = (unsigned)(selection_y2 - selection_y1 + 1);
    out = text;
    memcpy(out, "selection ", 10u);
    out += 10;
    out = append_coord_value(out, (int)width);
    *out++ = 'x';
    out = append_coord_value(out, (int)height);
    *out = '\0';
    set_picker_status(text);
}

static void clear_selection(void)
{
    selection_hide_overlay();
    selection_active = false;
    selection_dragging = false;
    selection_x1 = 1;
    selection_y1 = 1;
    selection_x2 = 0;
    selection_y2 = 0;
    draw_invert_button();
    draw_mirror_button();
}

static void selection_update_rect(int x1, int y1, int x2, int y2)
{
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 < 0) x2 = 0;
    if (y2 < 0) y2 = 0;
    if (x1 >= (int)GFX_CANVAS_WIDTH)  x1 = (int)GFX_CANVAS_WIDTH - 1;
    if (x2 >= (int)GFX_CANVAS_WIDTH)  x2 = (int)GFX_CANVAS_WIDTH - 1;
    if (y1 >= (int)GFX_CANVAS_HEIGHT) y1 = (int)GFX_CANVAS_HEIGHT - 1;
    if (y2 >= (int)GFX_CANVAS_HEIGHT) y2 = (int)GFX_CANVAS_HEIGHT - 1;
    normalize_rect(&x1, &y1, &x2, &y2);
    selection_x1 = x1;
    selection_y1 = y1;
    selection_x2 = x2;
    selection_y2 = y2;
}

static void selection_begin_drag(int x, int y)
{
    selection_hide_overlay();
    selection_active = false;
    selection_dragging = true;
    selection_anchor_x = x;
    selection_anchor_y = y;
    selection_update_rect(x, y, x, y);
    selection_show_overlay();
    draw_invert_button();
    draw_mirror_button();
    update_selection_size_status();
}

static void selection_update_drag(int x, int y)
{
    if (!selection_dragging)
        return;
    selection_hide_overlay();
    selection_update_rect(selection_anchor_x, selection_anchor_y, x, y);
    selection_show_overlay();
    update_selection_size_status();
}

static void selection_finish_drag(void)
{
    if (!selection_dragging)
        return;
    selection_hide_overlay();
    selection_dragging = false;
    selection_active = true;
    selection_show_overlay();
    draw_invert_button();
    draw_mirror_button();
    set_picker_status("selected");
}

static uint16_t clipboard_read_u16(unsigned offset)
{
    return (uint16_t)clipboard_header[offset] |
           (uint16_t)((uint16_t)clipboard_header[offset + 1u] << 8);
}

static void clipboard_write_u16(unsigned offset, uint16_t value)
{
    clipboard_header[offset] = (uint8_t)(value & 0xFFu);
    clipboard_header[offset + 1u] = (uint8_t)(value >> 8);
}

static uint8_t clipboard_open_read(int *fd)
{
    *fd = open(clipboard_path, O_RDONLY);
    if (*fd < 0)
        return 0;
    if (read(*fd, clipboard_header, sizeof(clipboard_header)) != (int)sizeof(clipboard_header))
    {
        close(*fd);
        *fd = -1;
        return 0;
    }
    if (clipboard_header[0] != 'P' || clipboard_header[1] != 'H' ||
        clipboard_header[2] != 'C' || clipboard_header[3] != 'B')
    {
        close(*fd);
        *fd = -1;
        return 0;
    }
    clipboard_width = clipboard_read_u16(4u);
    clipboard_height = clipboard_read_u16(6u);
    clipboard_stride = clipboard_read_u16(8u);
    if (clipboard_width == 0 || clipboard_height == 0 ||
        clipboard_width > GFX_CANVAS_WIDTH ||
        clipboard_height > GFX_CANVAS_HEIGHT ||
        clipboard_stride == 0 || clipboard_stride > CANVAS_STRIDE)
    {
        close(*fd);
        *fd = -1;
        return 0;
    }
    clipboard_valid = 1u;
    return 1u;
}

static void clipboard_build_preview_outline(int fd)
{
    int y;
    int x;
    int bytes_read;
    uint16_t left;
    uint16_t right;

    clipboard_preview_valid = 0u;
    clipboard_preview_top = clipboard_height;
    clipboard_preview_bottom = 0u;
    clipboard_preview_left_x = clipboard_width;
    clipboard_preview_right_x = 0u;

    for (y = 0; y < (int)clipboard_height; y++)
    {
        if ((y & 0x0Fu) == 0 && operation_cancel_requested())
            return;
        bytes_read = read(fd, clipboard_row, clipboard_stride);
        if (bytes_read != (int)clipboard_stride)
            return;

        left = clipboard_width;
        right = 0u;
        for (x = 0; x < (int)clipboard_width; x++)
        {
            if ((clipboard_row[(unsigned)x >> 3] & (uint8_t)(0x80u >> (x & 7))) == 0u)
                continue;
            if (left == clipboard_width)
                left = (uint16_t)x;
            right = (uint16_t)x;
        }

        if (left != clipboard_width)
        {
            if (left < clipboard_preview_left_x)
                clipboard_preview_left_x = left;
            if (right > clipboard_preview_right_x)
                clipboard_preview_right_x = right;
            if (!clipboard_preview_valid)
            {
                clipboard_preview_top = (uint16_t)y;
                clipboard_preview_bottom = (uint16_t)y;
                clipboard_preview_valid = 1u;
            }
            else
            {
                clipboard_preview_bottom = (uint16_t)y;
            }
        }
    }
}

static uint8_t clipboard_write_selection(void)
{
    int fd;
    int y;
    int x;
    int width;
    int height;

    if (!selection_active)
    {
        set_picker_status("NOSEL");
        return 0;
    }

    fd = open(clipboard_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
    {
        clipboard_valid = 0;
        set_picker_status("CLIPERR");
        return 0;
    }

    width = selection_x2 - selection_x1 + 1;
    height = selection_y2 - selection_y1 + 1;
    clipboard_header[0] = 'P';
    clipboard_header[1] = 'H';
    clipboard_header[2] = 'C';
    clipboard_header[3] = 'B';
    clipboard_write_u16(4u, (uint16_t)width);
    clipboard_write_u16(6u, (uint16_t)height);
    clipboard_write_u16(8u, (uint16_t)((width + 7) / 8));

    selection_hide_overlay();
    if (write(fd, clipboard_header, sizeof(clipboard_header)) != (int)sizeof(clipboard_header))
    {
        close(fd);
        selection_show_overlay();
        clipboard_valid = 0;
        set_picker_status("CLIPERR");
        return 0;
    }

    for (y = 0; y < height; y++)
    {
        if ((y & 0x0Fu) == 0 && operation_cancel_requested())
        {
            close(fd);
            remove(clipboard_path);
            selection_show_overlay();
            clipboard_valid = 0;
            return 0;
        }
        memset(clipboard_row, 0, clipboard_read_u16(8u));
        for (x = 0; x < width; x++)
        {
            if (raw_get_pixel(selection_x1 + x, selection_y1 + y))
                clipboard_row[(unsigned)x >> 3] |= (uint8_t)(0x80u >> (x & 7));
        }
        if (write(fd, clipboard_row, clipboard_read_u16(8u)) != (int)clipboard_read_u16(8u))
        {
            close(fd);
            selection_show_overlay();
            clipboard_valid = 0;
            set_picker_status("CLIPERR");
            return 0;
        }
    }

    close(fd);
    selection_show_overlay();
    clipboard_width = (uint16_t)width;
    clipboard_height = (uint16_t)height;
    clipboard_stride = clipboard_read_u16(8u);
    clipboard_valid = 1u;
    return 1u;
}

static uint8_t paste_preview_pixel(int x, int y)
{
    return ((x + y) & 1) != 0;
}

static void paste_preview_toggle_overlay(void)
{
    int x1;
    int y1;
    int x2;
    int y2;
    int x;
    int y;
    int px1;
    int px2;

    if (!paste_preview_active || clipboard_width == 0 || clipboard_height == 0)
        return;

    x1 = paste_x;
    y1 = paste_y;
    x2 = paste_x + (int)clipboard_width - 1;
    y2 = paste_y + (int)clipboard_height - 1;

    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= (int)GFX_CANVAS_WIDTH)  x2 = (int)GFX_CANVAS_WIDTH - 1;
    if (y2 >= (int)GFX_CANVAS_HEIGHT) y2 = (int)GFX_CANVAS_HEIGHT - 1;
    if (x1 > x2 || y1 > y2)
        return;

    for (x = x1; x <= x2; x++)
    {
        if (paste_preview_pixel(x, y1))
            raw_toggle_pixel(x, y1);
        if (y2 != y1 && paste_preview_pixel(x, y2))
            raw_toggle_pixel(x, y2);
    }
    for (y = y1 + 1; y < y2; y++)
    {
        if (paste_preview_pixel(x1, y))
            raw_toggle_pixel(x1, y);
        if (x2 != x1 && paste_preview_pixel(x2, y))
            raw_toggle_pixel(x2, y);
    }

    if (!clipboard_preview_valid)
        return;

    px1 = paste_x + (int)clipboard_preview_left_x;
    px2 = paste_x + (int)clipboard_preview_right_x;
    y1 = paste_y + (int)clipboard_preview_top;
    y2 = paste_y + (int)clipboard_preview_bottom;

    if (px1 < 0) px1 = 0;
    if (y1 < 0) y1 = 0;
    if (px2 >= (int)GFX_CANVAS_WIDTH)  px2 = (int)GFX_CANVAS_WIDTH - 1;
    if (y2 >= (int)GFX_CANVAS_HEIGHT)  y2 = (int)GFX_CANVAS_HEIGHT - 1;

    if (px1 <= px2 && y1 <= y2)
    {
        for (x = px1; x <= px2; x++)
        {
            if (paste_preview_pixel(x, y1))
                raw_toggle_pixel(x, y1);
            if (y2 != y1 && paste_preview_pixel(x, y2))
                raw_toggle_pixel(x, y2);
        }
        for (y = y1 + 1; y < y2; y++)
        {
            if (paste_preview_pixel(px1, y))
                raw_toggle_pixel(px1, y);
            if (px2 != px1 && paste_preview_pixel(px2, y))
                raw_toggle_pixel(px2, y);
        }
    }
}

static void paste_preview_hide(void)
{
    if (!paste_preview_visible)
        return;
    paste_preview_toggle_overlay();
    paste_preview_visible = false;
}

static void paste_preview_show(void)
{
    if (paste_preview_visible || !paste_preview_active)
        return;
    paste_preview_toggle_overlay();
    paste_preview_visible = true;
}

static void crosshair_toggle_overlay(void)
{
    int i;
    for (i = 0; i < (int)GFX_CANVAS_WIDTH; i += 8)
        raw_toggle_pixel(i, crosshair_y);
    for (i = 0; i < (int)GFX_CANVAS_HEIGHT; i += 8)
        raw_toggle_pixel(crosshair_x, i);
}

static void crosshair_hide(void)
{
    if (!crosshair_visible)
        return;
    crosshair_toggle_overlay();
    crosshair_visible = false;
}

static void crosshair_show(void)
{
    if (!crosshair_active || crosshair_visible)
        return;
    crosshair_x = mouse_pos_x;
    crosshair_y = mouse_pos_y;
    crosshair_toggle_overlay();
    crosshair_visible = true;
}

static void line_anchor_toggle_marker(void)
{
    int d;
    for (d = 1; d <= 4; d++)
    {
        raw_toggle_pixel(line_anchor_marker_x - d, line_anchor_marker_y - d);
        raw_toggle_pixel(line_anchor_marker_x + d, line_anchor_marker_y - d);
        raw_toggle_pixel(line_anchor_marker_x - d, line_anchor_marker_y + d);
        raw_toggle_pixel(line_anchor_marker_x + d, line_anchor_marker_y + d);
    }
}

static void line_anchor_hide_marker(void)
{
    if (!line_anchor_marker_visible)
        return;
    line_anchor_toggle_marker();
    line_anchor_marker_visible = false;
}

static void line_anchor_show_marker(void)
{
    if (line_anchor_marker_visible)
        return;
    line_anchor_marker_x = line_anchor_x;
    line_anchor_marker_y = line_anchor_y;
    line_anchor_toggle_marker();
    line_anchor_marker_visible = true;
}

static void zoom_area_toggle(void)
{
    int x, y;
    int x2 = zoom_area_marker_x + ZOOM_AREA - 1;
    int y2 = zoom_area_marker_y + ZOOM_AREA - 1;
    for (x = zoom_area_marker_x; x <= x2; x++)
    {
        raw_toggle_pixel(x, zoom_area_marker_y);
        raw_toggle_pixel(x, y2);
    }
    for (y = zoom_area_marker_y + 1; y < y2; y++)
    {
        raw_toggle_pixel(zoom_area_marker_x, y);
        raw_toggle_pixel(x2, y);
    }
}

static void zoom_area_hide(void)
{
    if (!zoom_area_visible)
        return;
    zoom_area_toggle();
    zoom_area_visible = false;
}

static void zoom_area_show(void)
{
    if (zoom_area_visible)
        return;
    zoom_area_marker_x = zoom_area_x;
    zoom_area_marker_y = zoom_area_y;
    zoom_area_toggle();
    zoom_area_visible = true;
}

static void zoom_redraw_icons(void)
{
    draw_swap_button();
    draw_erase_button();
    draw_invert_button();
    draw_mirror_button();
    draw_select_button();
    draw_rect_tool_button();
    draw_ellipse_tool_button();
    draw_all_shape_buttons();
    draw_picker_text_button("-", PICKER_MINUS_X);
    draw_picker_text_button("+", PICKER_PLUS_X);
}

static void zoom_area_pixels_save(void)
{
    int sy, bx;
    unsigned src_byte;
    uint8_t byte_val;

    RIA.step1 = 1;
    RIA.addr1 = ZOOM_AREA_BUF_ADDR;
    for (sy = 0; sy < ZOOM_AREA; sy++)
    {
        src_byte = (unsigned)zoom_area_y * CANVAS_STRIDE
                   + (unsigned)zoom_area_x / 8u
                   + (unsigned)sy * CANVAS_STRIDE;
        for (bx = 0; bx < ZOOM_AREA / 8; bx++)
        {
            RIA.step0 = 0;
            RIA.addr0 = src_byte + (unsigned)bx;
            byte_val = RIA.rw0;
            RIA.rw1 = byte_val;
        }
    }
}

static void zoom_enter_view(void)
{
//    zoom_saved_picker_x = picker_x;
//    zoom_saved_picker_y = picker_y;
    // move_picker(0, (int)GFX_CANVAS_HEIGHT - PICKER_HEIGHT);
    zoom_view_active = true;
    zoom_redraw_icons();
}

static void zoom_exit_view(void)
{
    zoom_view_active = false;
    // move_picker(zoom_saved_picker_x, zoom_saved_picker_y);
    zoom_redraw_icons();
}

static void zoom_cancel(void)
{
    if (zoom_area_active)
    {
        zoom_area_hide();
        zoom_area_active = false;
        set_picker_status("");
    }
    if (zoom_view_active)
    {
        busy_begin();
        snapshot_load_canvas("TMP/paintHD_zoom.bin");
        busy_end();
        zoom_exit_view();
        set_picker_status("");
    }
}

static void zoom_area_move(int x, int y)
{
    int nx, ny;
    nx = x - ZOOM_AREA / 2;
    ny = y - ZOOM_AREA / 2;
    if (nx < 0) nx = 0;
    if (nx > (int)(GFX_CANVAS_WIDTH  - ZOOM_AREA)) nx = (int)(GFX_CANVAS_WIDTH  - ZOOM_AREA);
    if (ny < 0) ny = 0;
    if (ny > (int)(GFX_CANVAS_HEIGHT - ZOOM_AREA)) ny = (int)(GFX_CANVAS_HEIGHT - ZOOM_AREA);
    if (nx == zoom_area_x && ny == zoom_area_y)
        return;
    zoom_area_hide();
    zoom_area_x = nx;
    zoom_area_y = ny;
    zoom_area_show();
}

static void zoom_redraw_block(int sx, int sy)
{
    int py;
    uint8_t v, bv;
    unsigned row_addr;

    RIA.addr1 = ZOOM_BUF_ADDR + (unsigned)sy * ZOOM_AREA + (unsigned)sx;
    RIA.step1 = 0;
    v = RIA.rw1;

    for (py = 0; py < ZOOM_DOT; py++)
    {
        row_addr = (unsigned)(ZOOM_FRAME_Y0 + (sy + 1) * ZOOM_DOT + py)
                   * CANVAS_STRIDE + 23u + 1u + (unsigned)sx;
        RIA.addr0 = row_addr;
        RIA.step0 = 0;
        bv = v ? 0xFF : 0x00;
        if (py == 1 || py == 3 || py == 5) bv ^= 0x01;
        if (py == 7) bv ^= 0x55;
        RIA.rw0 = bv;
    }
}

static void zoom_apply_changes(void)
{
    int sx, sy;
    uint8_t v;

    snapshot_load_canvas("TMP/paintHD_zoom.bin");
    RIA.addr1 = ZOOM_BUF_ADDR;
    RIA.step1 = 1;
    for (sy = 0; sy < ZOOM_AREA; sy++)
        for (sx = 0; sx < ZOOM_AREA; sx++)
        {
            v = RIA.rw1;
            raw_set_pixel(zoom_area_x + sx, zoom_area_y + sy, v ? 1 : 0);
        }
    zoom_exit_view();
    set_picker_status("");
}

static void zoom_draw_view(void)
{
    int sx, sy, py, i, cy;
    uint8_t v, byte_val;
    unsigned row_addr;

    /* read source pixels from XRAM backup into zoom scratch (port 1) */
    {
        uint8_t b;
        int bit;
        RIA.addr1 = ZOOM_BUF_ADDR;
        RIA.step1 = 1;
        for (sy = 0; sy < ZOOM_AREA; sy++)
        {
            for (sx = 0; sx < ZOOM_AREA; sx += 8)
            {
                RIA.step0 = 0;
                RIA.addr0 = ZOOM_AREA_BUF_ADDR
                            + (unsigned)sy * (ZOOM_AREA / 8u)
                            + (unsigned)(sx / 8);
                b = RIA.rw0;
                for (bit = 7; bit >= 0; bit--)
                    RIA.rw1 = (b >> bit) & 1u;
            }
        }
    }

    /* clear full area + 1px border:
       ZOOM_FRAME_X0=184=23*8 byte-aligned, ZOOM_FRAME_W=272=34*8
       bytes 23..56 (34 bytes), rows 103..376 (274 rows) */
    for (cy = ZOOM_FRAME_Y0 - 1; cy <= ZOOM_FRAME_Y0 + ZOOM_FRAME_H; cy++)
    {
        RIA.addr0 = (unsigned)cy * CANVAS_STRIDE + 23u;
        RIA.step0 = 1;
        for (i = 0; i < 34; i++)
            RIA.rw0 = 0;
    }

    /* draw 34x34 blocks (sy=-1..32: top frame + 32 pixel rows + bottom frame)
       port 0: byte write (addr0+step0=1, 34 bytes/row)
       port 1: XRAM read  (addr1+step1=1, 32 pixels/row)
       ports 0 and 1 are independent — safe to use simultaneously
       grid via XOR on byte_val:
         vertical:   py=1,3,5 -> XOR 0x01 (bit 0 = right pixel of block)
         horizontal: py=7     -> XOR 0x55 (bits 6,4,2,0 = px 1,3,5,7) */
    for (sy = -1; sy <= ZOOM_AREA; sy++)
    {
        for (py = 0; py < ZOOM_DOT; py++)
        {
            row_addr = (unsigned)(ZOOM_FRAME_Y0 + (sy + 1) * ZOOM_DOT + py)
                       * CANVAS_STRIDE + 23u;
            RIA.addr0 = row_addr;
            RIA.step0 = 1;

            if (sy >= 0 && sy < ZOOM_AREA)
            {
                RIA.rw0 = (py % 2); /* left frame block */
                RIA.addr1 = ZOOM_BUF_ADDR + (unsigned)sy * ZOOM_AREA;
                RIA.step1 = 1;
                for (sx = 0; sx < ZOOM_AREA; sx++)
                {
                    v = RIA.rw1;
                    byte_val = v ? 0xFF : 0x00;
                    if (py == 1 || py == 3 || py == 5) byte_val ^= 0x01;
                    if (py == 7) byte_val ^= 0x55;
                    RIA.rw0 = byte_val;
                }
                RIA.rw0 = 0x00; /* right frame block */
            }
            else
            {
                for (i = 0; i < 33; i++){
                    if(sy == -1 && py == 7){
                        RIA.rw0 = (i >= 1 && i <= 32) ? 0x55 : 0x01;
                    } else {
                        RIA.rw0 = 0x00;
                    }
                }
            }
        }
    }
    draw_canvas_text(" ZOOM ",
                     ZOOM_FRAME_X0, (ZOOM_FRAME_Y0 - 2u), (GFX_CANVAS_WIDTH - 1), BLACK, WHITE);
    draw_canvas_text("LMB toggle ENTER confirm ESC abandon",
                     ZOOM_FRAME_X0 + 8, (ZOOM_FRAME_Y0 + ZOOM_FRAME_H - 4), (GFX_CANVAS_WIDTH - 1), WHITE, BLACK);

}

static void paste_preview_cancel(void)
{
    paste_preview_hide();
    paste_preview_active = false;
    paste_transparent = false;
    clipboard_preview_valid = 0u;
    set_picker_status("");
    selection_show_overlay();
}

static void paste_preview_begin(uint8_t transparent)
{
    int fd;

    paste_preview_cancel();
    busy_begin();
    operation_cancel_begin();
    if (!clipboard_open_read(&fd))
    {
        clipboard_valid = 0;
        busy_end();
        set_picker_status("NOCLIP");
        return;
    }
    clipboard_build_preview_outline(fd);
    close(fd);
    if (operation_was_cancelled())
    {
        busy_end();
        set_picker_status("cancelled");
        return;
    }
    busy_end();
    selection_hide_overlay();
    paste_preview_active = true;
    paste_preview_visible = false;
    paste_transparent = transparent != 0;
    paste_x = mouse_pos_x;
    paste_y = mouse_pos_y;
    paste_preview_show();
    if (paste_transparent)
        set_picker_status("paste transparent (LMB)");
    else
        set_picker_status("paste (LMB)");
}

static void paste_preview_move(int x, int y)
{
    if (!paste_preview_active)
        return;
    paste_preview_hide();
    paste_x = x;
    paste_y = y;
    paste_preview_show();
}

static void clipboard_cut_selection(void)
{
    if (!selection_active)
    {
        set_picker_status("NOSEL");
        return;
    }
    busy_begin();
    operation_cancel_begin();
    if (!clipboard_write_selection())
    {
        busy_end();
        if (operation_was_cancelled())
            set_picker_status("cancelled");
        return;
    }
    if (!prepare_undo_step())
    {
        busy_end();
        return;
    }
    set_canvas_dirty(true);
    canvas_modify_begin();
    fill_rect(selection_x1, selection_y1, selection_x2, selection_y2, 0);
    canvas_modify_end();
    snapshot_refresh_current();
    busy_end();
    if (operation_was_cancelled())
        set_picker_status("cancelled");
    else
        set_picker_status("CUT");
}

static void clipboard_copy_selection(void)
{
    busy_begin();
    operation_cancel_begin();
    if (!clipboard_write_selection())
    {
        busy_end();
        if (operation_was_cancelled())
            set_picker_status("cancelled");
        return;
    }
    busy_end();
    set_picker_status("COPIED");
}

static void clipboard_paste_apply(void)
{
    int fd;
    int y;
    int x;
    int width;
    int height;
    uint8_t bit;
    uint8_t clip_was_active;

    if (!paste_preview_active)
        return;
    if (!clipboard_open_read(&fd))
    {
        clipboard_valid = 0;
        paste_preview_cancel();
        set_picker_status("NOCLIP");
        return;
    }
    if (!prepare_undo_step())
    {
        close(fd);
        return;
    }

    busy_begin();
    width = (int)clipboard_width;
    height = (int)clipboard_height;
    paste_preview_hide();
    clip_was_active = selection_active ? 1u : 0u;
    selection_active = false;
    canvas_modify_begin();
    for (y = 0; y < height; y++)
    {
        if ((y & 0x0Fu) == 0 && operation_cancel_requested())
        {
            close(fd);
            paste_preview_active = false;
            paste_preview_visible = false;
            selection_active = clip_was_active;
            canvas_modify_end();
            set_canvas_dirty(true);
            snapshot_refresh_current();
            busy_end();
            set_picker_status("cancelled");
            return;
        }
        if (read(fd, clipboard_row, clipboard_stride) != (int)clipboard_stride)
        {
            close(fd);
            paste_preview_active = false;
            paste_preview_visible = false;
            selection_active = clip_was_active;
            canvas_modify_end();
            clipboard_valid = 0;
            busy_end();
            set_picker_status("CLIPERR");
            return;
        }
        for (x = 0; x < width; x++)
        {
            bit = (clipboard_row[(unsigned)x >> 3] & (uint8_t)(0x80u >> (x & 7))) ? 1u : 0u;
            if (!paste_transparent || bit)
                set_pixel(paste_x + x, paste_y + y, bit);
        }
    }
    close(fd);
    paste_preview_active = false;
    paste_preview_visible = false;
    selection_active = clip_was_active;
    canvas_modify_end();
    set_canvas_dirty(true);
    snapshot_refresh_current();
    busy_end();
    set_picker_status("PASTED");
}

static void fill_rect(int x1, int y1, int x2, int y2, uint8_t color)
{
    int y, bx1, bx2, bx;
    uint8_t lmask, rmask, full;
    unsigned row_base;
    uint8_t mask;

    if (!selection_clip_rect(&x1, &y1, &x2, &y2))
        return;
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= (int)GFX_CANVAS_WIDTH)  x2 = (int)GFX_CANVAS_WIDTH - 1;
    if (y2 >= (int)GFX_CANVAS_HEIGHT) y2 = (int)GFX_CANVAS_HEIGHT - 1;
    if (x1 > x2 || y1 > y2) return;

    bx1 = x1 / 8;
    bx2 = x2 / 8;
    lmask = (uint8_t)(0xFF >> (x1 & 7));
    rmask = (uint8_t)(0xFF << (7 - (x2 & 7)));
    full  = color ? 0xFF : 0x00;
    row_base = (unsigned)y1 * CANVAS_STRIDE;

    for (y = y1; y <= y2; y++)
    {
        if (((unsigned)(y - y1) & 0x1Fu) == 0u && operation_cancel_requested())
            break;
        if (bx1 == bx2)
        {
            mask = (uint8_t)(lmask & rmask);
            RIA.step0 = 0;
            RIA.addr0 = row_base + (unsigned)bx1;
            if (color) RIA.rw0 = RIA.rw0 | mask;
            else       RIA.rw0 = RIA.rw0 & ~mask;
        }
        else
        {
            RIA.step0 = 0;
            RIA.addr0 = row_base + (unsigned)bx1;
            if (color) RIA.rw0 = RIA.rw0 | lmask;
            else       RIA.rw0 = RIA.rw0 & ~lmask;

            RIA.step0 = 1;
            RIA.addr0 = row_base + (unsigned)bx1 + 1;
            for (bx = bx1 + 1; bx < bx2; bx++)
                RIA.rw0 = full;

            RIA.step0 = 0;
            RIA.addr0 = row_base + (unsigned)bx2;
            if (color) RIA.rw0 = RIA.rw0 | rmask;
            else       RIA.rw0 = RIA.rw0 & ~rmask;
        }
        row_base += (unsigned)CANVAS_STRIDE;
    }
}

static void set_pixel(int x, int y, uint8_t color)
{
    if (!selection_point_allowed(x, y))
        return;
    raw_set_pixel(x, y, color);
}

static uint8_t get_pixel(int x, int y)
{
    return raw_get_pixel(x, y);
}

/* --------------------- Iterative scanline flood fill. --------------------- */
// Queue stores seed x values paired with their row y.
// For each seed: scan left and right while pixel == target,
// fill the span, then seed the adjacent rows for each
// contiguous sub-span found there.
static int fill_qx[FILL_QUEUE_SIZE];
static int fill_qy[FILL_QUEUE_SIZE];
static uint16_t fill_qsize;
static uint8_t fill_overflow;

// helper for flood_fill()
static void fill_push(int x, int y)
{
    if (fill_qsize >= FILL_QUEUE_SIZE)
    {
        fill_overflow = 1;
        return;
    }
    fill_qx[fill_qsize] = x;
    fill_qy[fill_qsize] = y;
    fill_qsize++;
}

// Seed adjacent row ny for each contiguous target span in [x1,x2].
// helper for flood_fill()
static void fill_seed_row(int x1, int x2, int ny, uint8_t target)
{
    int x;
    uint8_t in_span;
    if (ny < 0 || ny >= (int)GFX_CANVAS_HEIGHT) return;
    if (selection_active)
    {
        if (ny < selection_y1 || ny > selection_y2)
            return;
        if (x1 < selection_x1) x1 = selection_x1;
        if (x2 > selection_x2) x2 = selection_x2;
    }
    in_span = 0;
    for (x = x1; x <= x2; x++)
    {
        if (get_pixel(x, ny) == target)
        {
            if (!in_span)
            {
                fill_push(x, ny);
                in_span = 1;
            }
        }
        else
        {
            in_span = 0;
        }
    }
}

static int fill_scan_left(int x, int min_x, int y, uint8_t target)
{
    unsigned row_base;
    uint8_t byte_val;
    int bx;
    int nbx;

    row_base = (unsigned)y * CANVAS_STRIDE;
    bx = x / 8;
    RIA.step0 = 0;
    RIA.addr0 = row_base + (unsigned)bx;
    byte_val = RIA.rw0;

    while (x > min_x)
    {
        uint8_t mask;
        nbx = (x - 1) / 8;
        if (nbx != bx)
        {
            bx = nbx;
            RIA.step0 = 0;
            RIA.addr0 = row_base + (unsigned)bx;
            byte_val = RIA.rw0;
        }
        mask = (uint8_t)(0x80u >> ((x - 1) & 7));
        if (target ? !(byte_val & mask) : (byte_val & mask))
            break;
        x--;
    }
    return x;
}

static int fill_scan_right(int x, int max_x, int y, uint8_t target)
{
    unsigned row_base;
    uint8_t byte_val;
    int bx;
    int nbx;

    row_base = (unsigned)y * CANVAS_STRIDE;
    bx = x / 8;
    RIA.step0 = 0;
    RIA.addr0 = row_base + (unsigned)bx;
    byte_val = RIA.rw0;

    while (x < max_x)
    {
        uint8_t mask;
        nbx = (x + 1) / 8;
        if (nbx != bx)
        {
            uint8_t full_mask;
            int right_bit;
            bx = nbx;
            RIA.step0 = 0;
            RIA.addr0 = row_base + (unsigned)bx;
            byte_val = RIA.rw0;
            right_bit = (max_x < bx * 8 + 7) ? (max_x & 7) : 7;
            full_mask = (uint8_t)(0xFFu << (7 - right_bit));
            if (target ? ((byte_val & full_mask) == full_mask)
                       : ((byte_val & full_mask) == 0u))
            {
                x = bx * 8 + right_bit;
                continue;
            }
        }
        mask = (uint8_t)(0x80u >> ((x + 1) & 7));
        if (target ? !(byte_val & mask) : (byte_val & mask))
            break;
        x++;
    }
    return x;
}

static uint8_t fill_process_queue(uint8_t target, uint8_t fill_color)
{
    int x, x1, x2, y;
    int min_x;
    int max_x;
    uint8_t esc_throttle;

    min_x = selection_active ? selection_x1 : 0;
    max_x = selection_active ? selection_x2 : (int)GFX_CANVAS_WIDTH - 1;
    esc_throttle = 0;

    while (fill_qsize > 0)
    {
        if ((esc_throttle & 0x0Fu) == 0u && key_pressed(HID_ESCAPE))
            return 0;
        esc_throttle++;

        fill_qsize--;
        x = fill_qx[fill_qsize];
        y = fill_qy[fill_qsize];

        if (get_pixel(x, y) != target)
            continue;

        x1 = fill_scan_left(x, min_x, y, target);
        x2 = fill_scan_right(x, max_x, y, target);

        fill_rect(x1, y, x2, y, fill_color);
        fill_seed_row(x1, x2, y - 1, target);
        fill_seed_row(x1, x2, y + 1, target);
    }

    return 1;
}

static void flood_fill(int sx, int sy, uint8_t target, uint8_t fill_color)
{
    if (target == fill_color) return;
    if (sx < 0 || sx >= (int)GFX_CANVAS_WIDTH) return;
    if (sy < 0 || sy >= (int)GFX_CANVAS_HEIGHT) return;
    if (!selection_point_allowed(sx, sy)) return;
    if (get_pixel(sx, sy) != target) return;

    fill_qsize = 0;
    fill_overflow = 0;
    fill_push(sx, sy);
    if (!fill_process_queue(target, fill_color))
        return;
    if (fill_overflow)
        return;
}
/* ------------------------------------------------------------------------- */

static void update_circle_cache(void)
{
    int py, px;
    int dx2, dy2;
    int cx2, cy2, r2;
    int span_start;
    int span_end;

    if (circle_cached_size == brush_size)
        return;

    cx2 = brush_size - 1;
    cy2 = brush_size - 1;
    r2 = brush_size * brush_size;

    for (py = 0; py < brush_size; py++)
    {
        dy2 = (py + py) - cy2;
        span_start = -1;
        span_end = -1;
        for (px = 0; px < brush_size; px++)
        {
            dx2 = (px + px) - cx2;
            if (dx2 * dx2 + dy2 * dy2 < r2)
            {
                if (span_start < 0)
                    span_start = px;
                span_end = px;
            }
        }
        if (span_start < 0)
        {
            circle_row_left[py] = 0;
            circle_row_right[py] = 0;
        }
        else
        {
            circle_row_left[py] = (uint8_t)span_start;
            circle_row_right[py] = (uint8_t)span_end;
        }
    }

    circle_cached_size = brush_size;
}

static void draw_brush_shape(uint8_t shape, int x, int y)
{
    int half;
    int start_x, start_y, end_x, end_y;
    int py;
    int dx;
    int sdx, sdy;
    int span_width;
    uint8_t row;
    uint8_t target;

    half = brush_size / 2;
    start_x = x - half;
    start_y = y - half;
    end_x = start_x + brush_size - 1;
    end_y = start_y + brush_size - 1;

    switch (shape)
    {
    case SHAPE_SQUARE:
        fill_rect(start_x, start_y, end_x, end_y, active_color);
        break;

    case SHAPE_CIRCLE:
        update_circle_cache();
        for (py = start_y; py <= end_y; py++)
        {
            row = (uint8_t)(py - start_y);
            fill_rect(start_x + circle_row_left[row], py,
                      start_x + circle_row_right[row], py,
                      active_color);
        }
        break;

    case SHAPE_VERT:
        if (vert_brush_flipped)
            fill_rect(start_x, y, end_x, y, active_color);
        else
            fill_rect(x, start_y, x, end_y, active_color);
        break;

    case SHAPE_DIAG:
        for (dx = 0; dx < brush_size; dx++)
        {
            if (diag_brush_flipped)
                set_pixel(end_x - dx, start_y + dx, active_color);
            else
                set_pixel(start_x + dx, start_y + dx, active_color);
        }
        break;

    case SHAPE_SPRAY:
        update_circle_cache();
        if (brush_size <= 10)
            target = 4u;
        else if (brush_size <= 20)
            target = 4u;
        else if (brush_size <= 36)
            target = 4u;
        else if (brush_size <= 52)
            target = 4u;
        else
            target = 32u;
        while (target != 0u)
        {
            row = (uint8_t)(prng_next() % brush_size);
            span_width = (int)circle_row_right[row] - (int)circle_row_left[row] + 1;
            if (span_width <= 0)
                continue;
            sdy = start_y + row;
            sdx = start_x + circle_row_left[row] + (int)(prng_next() % (unsigned)span_width);
            set_pixel(sdx, sdy, active_color);
            target--;
        }
        break;
    }
}

static void draw_brush(int x, int y)
{
    draw_brush_shape(brush_shape, x, y);
}

static void start_canvas_draw(uint8_t button, uint8_t color, int x, int y)
{
    drawing_button = button;
    active_color = color;
    is_drawing = true;
    line_x = x;
    line_y = y;
    set_canvas_dirty(true);
    draw_brush(x, y);
}

static uint8_t draw_line_square_fast(int x0, int y0, int x1, int y1)
{
    int half;

    half = brush_size / 2;

    if (y0 == y1)
    {
        fill_rect(
            (x0 < x1 ? x0 : x1) - half, y0 - half,
            (x0 < x1 ? x1 : x0) + half, y0 + half,
            active_color);
        return 1u;
    }
    if (x0 == x1)
    {
        fill_rect(
            x0 - half, (y0 < y1 ? y0 : y1) - half,
            x0 + half, (y0 < y1 ? y1 : y0) + half,
            active_color);
        return 1u;
    }
    return 0u;
}

static uint8_t draw_line_vert_fast(int x0, int y0, int x1, int y1)
{
    int half;

    half = brush_size / 2;

    if (vert_brush_flipped)
    {
        if (y0 == y1)
        {
            fill_rect(
                (x0 < x1 ? x0 : x1) - half, y0,
                (x0 < x1 ? x1 : x0) + half, y0,
                active_color);
            return 1u;
        }
        return 0u;
    }
    else
    {
        if (x0 == x1)
        {
            fill_rect(
                x0, (y0 < y1 ? y0 : y1) - half,
                x0, (y0 < y1 ? y1 : y0) + half,
                active_color);
            return 1u;
        }
        return 0u;
    }
}

static uint8_t draw_line_circle_fast(int x0, int y0, int x1, int y1)
{
    int half;
    uint8_t row;

    update_circle_cache();
    half = brush_size / 2;

    if (y0 == y1)
    {
        if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
        for (row = 0; row < (uint8_t)brush_size; row++)
        {
            int py  = y0 - half + (int)row;
            int fx1 = x0 - half + (int)circle_row_left[row];
            int fx2 = x1 - half + (int)circle_row_right[row];
            fill_rect(fx1, py, fx2, py, active_color);
        }
        return 1u;
    }
    if (x0 == x1)
    {
        if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
        for (row = 0; row < (uint8_t)brush_size; row++)
        {
            int fy1 = y0 - half + (int)row;
            int fy2 = y1 - half + (int)row;
            int fx1 = x0 - half + (int)circle_row_left[row];
            int fx2 = x0 - half + (int)circle_row_right[row];
            fill_rect(fx1, fy1, fx2, fy2, active_color);
        }
        return 1u;
    }
    return 0u;
}

static void draw_line_brush_shape(uint8_t shape, int x0, int y0, int x1, int y1)
{
    int dx, dy;
    int ax, ay;
    int sx, sy;
    int cx, cy;

    dx = x1 - x0;
    dy = y1 - y0;
    ax = dx < 0 ? -dx : dx;
    ay = dy < 0 ? -dy : dy;
    sx = dx < 0 ? -1 : 1;
    sy = dy < 0 ? -1 : 1;
    cx = x0;
    cy = y0;

    if (shape == SHAPE_SQUARE && draw_line_square_fast(x0, y0, x1, y1))
        return;
    if (shape == SHAPE_VERT && draw_line_vert_fast(x0, y0, x1, y1))
        return;
    if (shape == SHAPE_CIRCLE && draw_line_circle_fast(x0, y0, x1, y1))
        return;

    if (ax > ay)
    {
        int d = 2 * ay - ax;
        while (cx != x1)
        {
            if (operation_cancel_requested())
                return;
            draw_brush_shape(shape, cx, cy);
            if (d > 0) { cy += sy; d -= 2 * ax; }
            d += 2 * ay;
            cx += sx;
        }
    }
    else
    {
        int d = 2 * ax - ay;
        while (cy != y1)
        {
            if (operation_cancel_requested())
                return;
            draw_brush_shape(shape, cx, cy);
            if (d > 0) { cx += sx; d -= 2 * ay; }
            d += 2 * ax;
            cy += sy;
        }
    }

    if (operation_cancel_requested())
        return;
    draw_brush_shape(shape, x1, y1);
}

static void draw_line_brush(int x0, int y0, int x1, int y1)
{
    draw_line_brush_shape(brush_shape, x0, y0, x1, y1);
}

static void draw_freehand_brush(int x0, int y0, int x1, int y1)
{
    int dx, dy, ax, ay;
    int step, dist, d2, num_steps, i;
    long s, t;

    dx = x1 - x0;
    dy = y1 - y0;
    ax = dx < 0 ? -dx : dx;
    ay = dy < 0 ? -dy : dy;

    s = (long)ax * ax + (long)ay * ay;
    if (s == 0) { draw_brush_shape(brush_shape, x1, y1); return; }

    /* isqrt via Newton's method */
    dist = ax > ay ? ax : ay;
    do {
        d2 = (int)(s / (long)dist + dist) / 2;
        if (d2 >= dist) break;
        dist = d2;
    } while (1);

    step = brush_size / 2;
    if (step < 1) step = 1;
    num_steps = dist / step;

    for (i = 1; i <= num_steps; i++)
    {
        t = (long)i * step;
        draw_brush_shape(brush_shape,
                         x0 + (int)(t * dx / dist),
                         y0 + (int)(t * dy / dist));
    }
    draw_brush_shape(brush_shape, x1, y1);
}

static uint8_t primitive_stroke_shape(void)
{
    return brush_shape == SHAPE_FILL ? SHAPE_SQUARE : brush_shape;
}

static void primitive_draw_rect_shape(uint8_t shape, int x1, int y1, int x2, int y2)
{
    draw_line_brush_shape(shape, x1, y1, x2, y1);
    if (operation_was_cancelled())
        return;
    if (y2 != y1)
        draw_line_brush_shape(shape, x1, y2, x2, y2);
    if (operation_was_cancelled())
        return;
    if (x2 != x1)
    {
        draw_line_brush_shape(shape, x1, y1, x1, y2);
        if (operation_was_cancelled())
            return;
        draw_line_brush_shape(shape, x2, y1, x2, y2);
    }
}

static void primitive_draw_ellipse_shape(uint8_t shape, int x0, int y0, int x1, int y1)
{
    int row;
    int y;
    int left;
    int right;
    int first_row;
    int last_row;
    int prev_y;
    int prev_left;
    int prev_right;
    uint8_t have_prev;

    if (!primitive_ellipse_ready)
        primitive_build_ellipse_rows();
    if (!primitive_ellipse_ready)
    {
        if (operation_was_cancelled())
            return;
        primitive_draw_rect_shape(shape, x0, y0, x1, y1);
        return;
    }

    first_row = -1;
    last_row = -1;
    for (row = 0; row <= y1 - y0; row++)
    {
        if (primitive_ellipse_left[row] != 0xFFFFu)
        {
            if (first_row < 0)
                first_row = row;
            last_row = row;
        }
    }

    have_prev = 0u;
    for (row = 0; row <= y1 - y0; row++)
    {
        if (operation_cancel_requested())
            return;
        left = (int)primitive_ellipse_left[row];
        if (left == 0xFFFF)
            continue;
        right = (int)primitive_ellipse_right[row];
        y = primitive_y1 + row;
        if (!have_prev)
        {
            if (right != left)
                draw_line_brush_shape(shape, left, y, right, y);
            else
                draw_brush_shape(shape, left, y);
            prev_y = y;
            prev_left = left;
            prev_right = right;
            have_prev = 1u;
            continue;
        }
        draw_line_brush_shape(shape, prev_left, prev_y, left, y);
        if (operation_was_cancelled())
            return;
        if (right != left || prev_right != prev_left)
            draw_line_brush_shape(shape, prev_right, prev_y, right, y);
        if (operation_was_cancelled())
            return;
        if (row == last_row && right != left)
            draw_line_brush_shape(shape, left, y, right, y);
        if (operation_was_cancelled())
            return;
        prev_y = y;
        prev_left = left;
        prev_right = right;
    }
}

static void primitive_finish_drag(void)
{
    uint8_t shape;

    if (!primitive_dragging)
        return;

    shape = primitive_stroke_shape();
    if (!prepare_undo_step())
    {
        primitive_hide_overlay();
        primitive_dragging = false;
        drawing_button = DRAW_BUTTON_NONE;
        return;
    }
    primitive_hide_overlay();
    primitive_dragging = false;
    busy_begin();
    set_canvas_dirty(true);
    operation_cancel_begin();
    canvas_modify_begin();
    if (active_tool == TOOL_ELLIPSE)
        primitive_draw_ellipse_shape(shape, primitive_x1, primitive_y1, primitive_x2, primitive_y2);
    else
        primitive_draw_rect_shape(shape, primitive_x1, primitive_y1, primitive_x2, primitive_y2);
    canvas_modify_end();
    snapshot_refresh_current();
    drawing_button = DRAW_BUTTON_NONE;
    busy_end();
    if (operation_was_cancelled())
        set_picker_status("cancelled");
    else
        set_picker_status(active_tool == TOOL_ELLIPSE ? "ellipse" : "rectangle");
}

static void constrain_line_axis(int x0, int y0, int *x1, int *y1)
{
    int dx, dy;
    int ax, ay;

    dx = *x1 - x0;
    dy = *y1 - y0;
    ax = dx < 0 ? -dx : dx;
    ay = dy < 0 ? -dy : dy;

    if (ax >= ay)
        *y1 = y0;
    else
        *x1 = x0;
}

// ---- Picker (8bpp overlay, 2x scaled) ----

static void draw_picker_box(uint8_t color, int x1, int y1, int x2, int y2)
{
    int x, y, tmp;
    if (x1 > x2) { tmp = x1; x1 = x2; x2 = tmp; }
    if (y1 > y2) { tmp = y1; y1 = y2; y2 = tmp; }
    RIA.step0 = 1;
    for (y = y1; y <= y2; y++)
    {
        RIA.addr0 = PICKER_DATA + PICKER_WIDTH * y + x1;
        for (x = x1; x <= x2; x++)
            RIA.rw0 = color;
    }
}

static char *append_coord_value(char *out, int value)
{
    unsigned v;

    v = (unsigned)value;
    if (v >= 100u)
    {
        *out++ = (char)('0' + (v / 100u));
        v %= 100u;
        *out++ = (char)('0' + (v / 10u));
        *out++ = (char)('0' + (v % 10u));
    }
    else if (v >= 10u)
    {
        *out++ = (char)('0' + (v / 10u));
        *out++ = (char)('0' + (v % 10u));
    }
    else
    {
        *out++ = (char)('0' + v);
    }
    return out;
}

static void draw_font_table(int ox, int oy)
{
    int col, row;
    unsigned int code;
    int px, py;

    for (row = 0; row < 16; row++)
    {
        for (col = 0; col < 16; col++)
        {
            code = (unsigned int)(row * 16 + col);
            if (code > 254u)
                break;
            px = ox + col * 6;
            py = oy + row * 10;
            draw_canvas_text_char((char)code, px, py, 1u, 0u);
        }
    }
}

static void draw_canvas_text_char(char ch, int px, int py, uint8_t fg_color, uint8_t bg_color)
{
    unsigned char code;
    int row, col;
    uint8_t bits;
    uint8_t mask;

    code = (unsigned char)ch;
    for (col = 0; col < 6; col++)
        raw_set_pixel(px + col, py-1, bg_color);
    for (row = 0; row < 8; row++)
    {
        mask = (uint8_t)(1u << row);
        for (col = 0; col < 5; col++)
        {
            RIA.step0 = 0;
            RIA.addr0 = XRAM_FONT5x7_ADDR + (unsigned)code * XRAM_FONT5x7_GLYPH_SIZE + (unsigned)col;
            bits = RIA.rw0;
            raw_set_pixel(px + col, py + row, (bits & mask) ? fg_color : bg_color);
        }
        raw_set_pixel(px + 5, py + row, bg_color);
    }
    for (col = 0; col < 6; col++)
        raw_set_pixel(px + col, py + 8, bg_color);
}

static void draw_canvas_text(const char *text, int px, int py, int max_x, uint8_t fg_color, uint8_t bg_color)
{
    while (*text != '\0' && (px + 4) <= max_x)
    {
        draw_canvas_text_char(*text, px, py, fg_color, bg_color);
        px += 6;
        text++;
    }
}

/* unused but do not touch
static int text_width(const char *text)
{
    int width;

    width = 0;
    while (*text != '\0')
    {
        width += 6;
        text++;
    }
    if (width != 0)
        width--;
    return width;
}
*/

#ifndef XRAM_FONT5x7
static void draw_picker_text_char(char ch, int px, int py, uint8_t fg_color, uint8_t bg_color)
{
    const uint8_t *glyph;
    unsigned char code;
    int row, col;
    uint8_t bits;
    uint8_t mask;
    uint8_t c;

    code = (unsigned char)ch;
    glyph = &ascii_font_5x7[(unsigned)code * 5u];
    for (row = 0; row < 8; row++)
    {
        for (col = 0; col < 5; col++)
        {
            bits = glyph[col];
            mask = (uint8_t)(1u << row);
            c = (bits & mask) ? fg_color : bg_color;
            RIA.step0 = 0;
            RIA.addr0 = PICKER_DATA + PICKER_WIDTH * (py + row) + (px + col);
            RIA.rw0 = c;
        }
    }
}
#else
static void draw_picker_text_char(char ch, int px, int py, uint8_t fg_color, uint8_t bg_color)
{
    unsigned char code;
    int row, col;
    uint8_t bits;
    uint8_t mask;

    code = (unsigned char)ch;
    for (row = 0; row < 8; row++)
    {
        for (col = 0; col < 5; col++)
        {
            RIA.step0 = 0;
            RIA.addr0 = XRAM_FONT5x7_ADDR + (unsigned)code * XRAM_FONT5x7_GLYPH_SIZE + (unsigned)col;
            bits = RIA.rw0;
            mask = (uint8_t)(1u << row);
            RIA.addr0 = PICKER_DATA + PICKER_WIDTH * (py + row) + (px + col);
            RIA.rw0 = (bits & mask) ? fg_color : bg_color;
        }
    }
}
#endif

static void draw_picker_text_colors(const char *text, int px, int py, uint8_t fg_color, uint8_t bg_color)
{
    while (*text)
    {
        draw_picker_text_char(*text, px, py, fg_color, bg_color);
        px += 6;
        text++;
    }
}

static void draw_picker_text(const char *text, int px, int py, uint8_t bg_color)
{
    draw_picker_text_colors(text, px, py, PICKER_FG_COLOR, bg_color);
}

static void draw_picker_title(void)
{
    draw_picker_box(PICKER_PANEL_COLOR, PICKER_TITLE_X, 1,
                    PICKER_TITLE_X + PICKER_TITLE_WIDTH - 1, 18);
    draw_picker_text("PaintHD", PICKER_TITLE_X + 4, 6, PICKER_PANEL_COLOR);
}

static void draw_picker_text_button(const char *text, int x)
{
    int px;

    draw_picker_box(PICKER_PANEL_COLOR, x, 1, x + PICKER_BUTTON_SIZE - 1, 18);
    px = x + (PICKER_BUTTON_SIZE - ((int)strlen(text) * 6 - 1)) / 2;
    draw_picker_text_colors(text, px, 6, ui_icon_color(), PICKER_PANEL_COLOR);
}

static void draw_mouse_coords(int x, int y)
{
    char text[12];
    char *out;
    int px;
    int py;

    out = text;
    if (zoom_view_active)
    {
        int bx = x - (int)ZOOM_VIEW_X0;
        int by = y - (int)ZOOM_VIEW_Y0;
        int sx = (bx >= 0 && bx < (int)ZOOM_VIEW_W) ? bx / ZOOM_DOT : -1;
        int sy = (by >= 0 && by < (int)ZOOM_VIEW_H) ? by / ZOOM_DOT : -1;
        *out++ = '[';
        if (sx >= 0 && sy >= 0)
        {
            out = append_coord_value(out, sx);
            *out++ = ',';
            out = append_coord_value(out, sy);
        }
        else
        {
            *out++ = '-';
        }
        *out++ = ']';
        *out = '\0';
    }
    else
    {
        *out++ = '(';
        out = append_coord_value(out, x);
        *out++ = ',';
        out = append_coord_value(out, y);
        *out++ = ')';
        *out = '\0';
    }

    draw_picker_box(PICKER_PANEL_COLOR, PICKER_COORDS_X, 1,
                    PICKER_COORDS_X + PICKER_COORDS_WIDTH - 1, 18);

    px = PICKER_COORDS_X + 4;
    py = 6;
    draw_picker_text(text, px, py, PICKER_PANEL_COLOR);
}

static void draw_picker_status(void)
{
    const char *text;
    int px;
    int py;
    int text_width;

    if (picker_status[0] != '\0')
        text = picker_status;
    else
        text = picker_hover_status ? picker_hover_status : "";
    draw_picker_box(PICKER_BG_COLOR, PICKER_STATUS_X, 1,
                    PICKER_STATUS_X + PICKER_STATUS_WIDTH - 1, 18);
    text_width = (int)strlen(text) * 6 - 1;
    if (text_width < 0)
        text_width = 0;
    px = PICKER_STATUS_X + 4;
    if (text_width > 0 && text_width < PICKER_STATUS_WIDTH - 8)
        px = PICKER_STATUS_X + (PICKER_STATUS_WIDTH - text_width) / 2;
    py = 6;
    draw_picker_text_colors(text, px, py, PICKER_FG_COLOR, PICKER_BG_COLOR);
}

static void set_picker_status(const char *text)
{
    strncpy(picker_status, text, sizeof(picker_status) - 1u);
    picker_status[sizeof(picker_status) - 1u] = '\0';
    if (picker_status[0] != '\0')
        picker_status_deadline = clock() + (clock_t)PICKER_STATUS_TICKS;
    else
        picker_status_deadline = 0;
    draw_picker_status();
}

static void set_picker_hover_status(const char *text)
{
    if (picker_hover_status != text)
    {
        picker_hover_status = text;
        draw_picker_status();
    }
}

static void set_canvas_dirty(bool dirty)
{
    if (canvas_dirty != dirty)
    {
        canvas_dirty = dirty;
        draw_save_button();
    }
}

static uint8_t canvas_input_locked(void)
{
    return startup_splash_pending != 0;
}

static const char *picker_hover_text(int x, int y)
{
    static const char *shape_text[SHAPE_COUNT] = {
        "brush sqare",
        "brush circle",
        0,
        0,
        "spray",
        "fill"
    };
    int shape;

    x -= picker_x;
    y -= picker_y;
    if (x < 0 || x >= PICKER_WIDTH || y < 0 || y >= PICKER_HEIGHT)
        return 0;
    if (x < 1 || x >= PICKER_WIDTH - 1 || y < 1 || y >= PICKER_HEIGHT - 1)
        return 0;
    if (x >= PICKER_HANDLE_X && x < PICKER_HANDLE_X + PICKER_BUTTON_SIZE)
        return "move toolbar";
    if (x >= PICKER_SAVE_X && x < PICKER_SAVE_X + PICKER_BUTTON_SIZE)
    {
        snprintf(picker_save_hover, sizeof(picker_save_hover), "save %s",
                 save_bmp_path ? save_bmp_path : "");
        return picker_save_hover;
    }
    if (x >= PICKER_SWAP_X && x < PICKER_SWAP_X + PICKER_BUTTON_SIZE)
        return "pick color";
    if (x >= PICKER_ERASE_X && x < PICKER_ERASE_X + PICKER_BUTTON_SIZE)
        return selection_mode_active() ? "wipe selection" : "wipe all";
    if (x >= PICKER_INVERT_X && x < PICKER_INVERT_X + PICKER_BUTTON_SIZE)
        return (active_tool == TOOL_SELECT || selection_active || selection_dragging)
                   ? "invert selection"
                   : "invert canvas";
    if (x >= PICKER_MIRROR_X && x < PICKER_MIRROR_X + PICKER_BUTTON_SIZE)
        return mirror_status_text();
    if (x >= PICKER_SELECT_X && x < PICKER_SELECT_X + PICKER_BUTTON_SIZE)
        return "select rectangle";
    if (x >= PICKER_RECT_X && x < PICKER_RECT_X + PICKER_BUTTON_SIZE)
    {
        if (!primitive_tools_enabled())
            return 0;
        return "draw rectangle";
    }
    if (x >= PICKER_ELLIPSE_X && x < PICKER_ELLIPSE_X + PICKER_BUTTON_SIZE)
    {
        if (!primitive_tools_enabled())
            return 0;
        return "draw ellipse";
    }
    if (x >= PICKER_SHAPE0_X && x < PICKER_SHAPE0_X + SHAPE_COUNT * PICKER_BUTTON_SIZE)
    {
        shape = (x - PICKER_SHAPE0_X) / PICKER_BUTTON_SIZE;
        if (!brush_shape_enabled((uint8_t)shape))
            return 0;
        if (shape == SHAPE_VERT)
            return vert_brush_flipped ? "brush horizontal" : "brush vertical";
        if (shape == SHAPE_DIAG)
            return diag_brush_flipped ? "brush diagonal R" : "brush diagonal L";
        return shape_text[shape];
    }
    if (x >= PICKER_MINUS_X && x < PICKER_MINUS_X + PICKER_BUTTON_SIZE)
        return "brush size -";
    if (x >= PICKER_PLUS_X && x < PICKER_PLUS_X + PICKER_BUTTON_SIZE)
        return "brush size +";
    return 0;
}

static void draw_size_display(void)
{
    char text[4];
    char *out;
    int px;
    int py = 6;

    out = text;
    out = append_coord_value(out, brush_size);
    *out = '\0';

    draw_picker_box(PICKER_PANEL_COLOR, PICKER_SIZE_X, 1, PICKER_SIZE_X + PICKER_BUTTON_SIZE - 1, 18);
    px = PICKER_SIZE_X + (PICKER_BUTTON_SIZE - (int)(strlen(text) * 6 - 1)) / 2;
    draw_picker_text(text, px, py, PICKER_PANEL_COLOR);
}

static uint8_t ui_icon_color(void)
{
    return zoom_view_active ? PICKER_SAVE_DISABLED_COLOR : PICKER_FG_COLOR;
}

static void draw_swap_button(void)
{
    int x, y;
    int row, col;
    uint8_t mirrored;
    uint16_t bits;
    uint8_t color;
    uint16_t mask;

    y = 2;
    x = PICKER_SWAP_X;
    mirrored = left_color ? 0u : 1u;

    draw_picker_box(PICKER_PANEL_COLOR, PICKER_SWAP_X, 1, PICKER_SWAP_X + PICKER_BUTTON_SIZE - 1, 18);
    for (row = 0; row < 16; row++)
    {
        bits = swap_icon[row];
        for (col = 0; col < 16; col++)
        {
            if (mirrored)
                mask = (uint16_t)(1u << col);
            else
                mask = (uint16_t)(0x8000u >> col);
            color = (bits & mask) ? ui_icon_color() : PICKER_PANEL_COLOR;
            RIA.step0 = 0;
            RIA.addr0 = PICKER_DATA + PICKER_WIDTH * (y + row) + (x + col);
            RIA.rw0 = color;
        }
    }
}

static void draw_erase_button(void)
{
    int row, col;
    int x, y;
    const uint16_t *icon;
    uint16_t bits;
    uint8_t color;

    x = PICKER_ERASE_X;
    y = 2;
    if (selection_mode_active())
        icon = left_color ? erase_icon_black_selected : erase_icon_white_selected;
    else
        icon = left_color ? erase_icon_black : erase_icon_white;
    draw_picker_box(PICKER_PANEL_COLOR, PICKER_ERASE_X, 1,
                    PICKER_ERASE_X + PICKER_BUTTON_SIZE - 1, 18);
    for (row = 0; row < 16; row++)
    {
        bits = icon[row];
        for (col = 0; col < 16; col++)
        {
            color = (bits & (uint16_t)(0x8000u >> col)) ? ui_icon_color() : PICKER_PANEL_COLOR;
            RIA.step0 = 0;
            RIA.addr0 = PICKER_DATA + PICKER_WIDTH * (y + row) + (x + col);
            RIA.rw0 = color;
        }
    }
}

static void draw_save_button(void)
{
    int row, col;
    uint16_t bits;
    uint8_t color;
    uint8_t icon_color;

    draw_picker_box(PICKER_PANEL_COLOR, PICKER_SAVE_X, 1,
                    PICKER_SAVE_X + PICKER_BUTTON_SIZE - 1, 18);
    icon_color = canvas_dirty ? PICKER_FG_COLOR : PICKER_SAVE_DISABLED_COLOR;
    for (row = 0; row < 16; row++)
    {
        bits = save_icon[row];
        for (col = 0; col < 16; col++)
        {
            color = (bits & (uint16_t)(0x8000u >> col)) ? icon_color : PICKER_PANEL_COLOR;
            RIA.step0 = 0;
            RIA.addr0 = PICKER_DATA + PICKER_WIDTH * (2 + row) + (PICKER_SAVE_X + col);
            RIA.rw0 = color;
        }
    }
}

static void draw_invert_button(void)
{
    int row;
    int col;
    uint16_t bits;
    uint8_t color;
    const uint16_t *icon;

    draw_picker_box(PICKER_PANEL_COLOR, PICKER_INVERT_X, 1,
                    PICKER_INVERT_X + PICKER_BUTTON_SIZE - 1, 18);
    icon = (active_tool == TOOL_SELECT || selection_active || selection_dragging)
               ? invert_selected_icon
               : invert_icon;
    for (row = 0; row < 16; row++)
    {
        bits = icon[row];
        for (col = 0; col < 16; col++)
        {
            color = (bits & (uint16_t)(0x8000u >> col)) ? ui_icon_color() : PICKER_PANEL_COLOR;
            RIA.step0 = 0;
            RIA.addr0 = PICKER_DATA + PICKER_WIDTH * (2 + row) + (PICKER_INVERT_X + col);
            RIA.rw0 = color;
        }
    }
}

static void draw_mirror_button(void)
{
    int row;
    int col;
    uint16_t bits;
    uint8_t color;
    const uint16_t *icon;

    draw_picker_box(PICKER_PANEL_COLOR, PICKER_MIRROR_X, 1,
                    PICKER_MIRROR_X + PICKER_BUTTON_SIZE - 1, 18);
    if (selection_mode_active())
        icon = mirror_vertical ? mirrorV_tool_selected_icon : mirrorH_tool_selected_icon;
    else
        icon = mirror_vertical ? mirrorV_tool_icon : mirrorH_tool_icon;
    for (row = 0; row < 16; row++)
    {
        bits = icon[row];
        for (col = 0; col < 16; col++)
        {
            color = (bits & (uint16_t)(0x8000u >> col)) ? ui_icon_color() : PICKER_PANEL_COLOR;
            RIA.step0 = 0;
            RIA.addr0 = PICKER_DATA + PICKER_WIDTH * (2 + row) + (PICKER_MIRROR_X + col);
            RIA.rw0 = color;
        }
    }
}

static void invert_region(int x1, int y1, int x2, int y2)
{
    int y;
    int bx1;
    int bx2;
    int bx;
    unsigned row_base;
    uint8_t lmask;
    uint8_t rmask;
    uint8_t mask;

    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= (int)GFX_CANVAS_WIDTH) x2 = (int)GFX_CANVAS_WIDTH - 1;
    if (y2 >= (int)GFX_CANVAS_HEIGHT) y2 = (int)GFX_CANVAS_HEIGHT - 1;
    if (x1 > x2 || y1 > y2)
        return;

    bx1 = x1 / 8;
    bx2 = x2 / 8;
    lmask = (uint8_t)(0xFFu >> (x1 & 7));
    rmask = (uint8_t)(0xFFu << (7 - (x2 & 7)));

    for (y = y1; y <= y2; y++)
    {
        if (((unsigned)(y - y1) & 0x1Fu) == 0u && operation_cancel_requested())
            break;
        row_base = (unsigned)y * CANVAS_STRIDE;
        if (bx1 == bx2)
        {
            mask = (uint8_t)(lmask & rmask);
            RIA.step0 = 0;
            RIA.addr0 = row_base + (unsigned)bx1;
            RIA.rw0 = RIA.rw0 ^ mask;
        }
        else
        {
            RIA.step0 = 0;
            RIA.addr0 = row_base + (unsigned)bx1;
            RIA.rw0 = RIA.rw0 ^ lmask;

            for (bx = bx1 + 1; bx < bx2; bx++)
            {
                RIA.step0 = 0;
                RIA.addr0 = row_base + (unsigned)bx;
                RIA.rw0 = RIA.rw0 ^ 0xFFu;
            }

            RIA.step0 = 0;
            RIA.addr0 = row_base + (unsigned)bx2;
            RIA.rw0 = RIA.rw0 ^ rmask;
        }
    }
}

static void invert_canvas_region(void)
{
    if (selection_active)
        invert_region(selection_x1, selection_y1, selection_x2, selection_y2);
    else
        invert_region(0, 0, (int)GFX_CANVAS_WIDTH - 1, (int)GFX_CANVAS_HEIGHT - 1);
}

static uint8_t reverse_bits8(uint8_t value)
{
    value = (uint8_t)(((value & 0xF0u) >> 4) | ((value & 0x0Fu) << 4));
    value = (uint8_t)(((value & 0xCCu) >> 2) | ((value & 0x33u) << 2));
    value = (uint8_t)(((value & 0xAAu) >> 1) | ((value & 0x55u) << 1));
    return value;
}

static void mirror_canvas_full_vertical(void)
{
    unsigned top_base;
    unsigned bottom_base;
    unsigned bx;
    int top_y;
    int bottom_y;
    uint8_t top_byte;
    uint8_t bottom_byte;

    for (top_y = 0; top_y < (int)GFX_CANVAS_HEIGHT / 2; top_y++)
    {
        if (((unsigned)top_y & 0x07u) == 0u && operation_cancel_requested())
            return;
        bottom_y = (int)GFX_CANVAS_HEIGHT - 1 - top_y;
        top_base = (unsigned)top_y * CANVAS_STRIDE;
        bottom_base = (unsigned)bottom_y * CANVAS_STRIDE;
        for (bx = 0; bx < CANVAS_STRIDE; bx++)
        {
            RIA.step0 = 0;
            RIA.addr0 = top_base + bx;
            top_byte = RIA.rw0;
            RIA.addr0 = bottom_base + bx;
            bottom_byte = RIA.rw0;
            RIA.addr0 = top_base + bx;
            RIA.rw0 = bottom_byte;
            RIA.addr0 = bottom_base + bx;
            RIA.rw0 = top_byte;
        }
    }
}

static void mirror_canvas_full_horizontal(void)
{
    unsigned row_base;
    unsigned left_bx;
    unsigned right_bx;
    int y;
    uint8_t left_byte;
    uint8_t right_byte;

    for (y = 0; y < (int)GFX_CANVAS_HEIGHT; y++)
    {
        if (((unsigned)y & 0x07u) == 0u && operation_cancel_requested())
            return;
        row_base = (unsigned)y * CANVAS_STRIDE;
        for (left_bx = 0, right_bx = CANVAS_STRIDE - 1u;
             left_bx < right_bx;
             left_bx++, right_bx--)
        {
            RIA.step0 = 0;
            RIA.addr0 = row_base + left_bx;
            left_byte = RIA.rw0;
            RIA.addr0 = row_base + right_bx;
            right_byte = RIA.rw0;
            RIA.addr0 = row_base + left_bx;
            RIA.rw0 = reverse_bits8(right_byte);
            RIA.addr0 = row_base + right_bx;
            RIA.rw0 = reverse_bits8(left_byte);
        }
    }
}

static void mirror_canvas_region(void)
{
    int x1;
    int y1;
    int x2;
    int y2;
    int x;
    int y;
    int other;
    uint8_t a;
    uint8_t b;

    if (selection_active)
    {
        x1 = selection_x1;
        y1 = selection_y1;
        x2 = selection_x2;
        y2 = selection_y2;
    }
    else
    {
        x1 = 0;
        y1 = 0;
        x2 = (int)GFX_CANVAS_WIDTH - 1;
        y2 = (int)GFX_CANVAS_HEIGHT - 1;
    }

    if (!selection_active)
    {
        if (mirror_vertical)
            mirror_canvas_full_vertical();
        else
            mirror_canvas_full_horizontal();
        return;
    }

    if (mirror_vertical)
    {
        for (y = y1; y <= (y1 + y2) / 2; y++)
        {
            if (((unsigned)(y - y1) & 0x07u) == 0u && operation_cancel_requested())
                return;
            other = y2 - (y - y1);
            if (other == y)
                continue;
            for (x = x1; x <= x2; x++)
            {
                a = raw_get_pixel(x, y);
                b = raw_get_pixel(x, other);
                raw_set_pixel(x, y, b);
                raw_set_pixel(x, other, a);
            }
        }
    }
    else
    {
        for (y = y1; y <= y2; y++)
        {
            if (((unsigned)(y - y1) & 0x07u) == 0u && operation_cancel_requested())
                return;
            for (x = x1; x <= (x1 + x2) / 2; x++)
            {
                other = x2 - (x - x1);
                if (other == x)
                    continue;
                a = raw_get_pixel(x, y);
                b = raw_get_pixel(other, y);
                raw_set_pixel(x, y, b);
                raw_set_pixel(other, y, a);
            }
        }
    }
}

static void toggle_mirror_mode(void)
{
    mirror_vertical = !mirror_vertical;
    draw_mirror_button();
}

static const char *mirror_status_text(void)
{
    if (selection_active)
        return mirror_vertical ? "mirror selection V" : "mirror selection H";
    return mirror_vertical ? "mirror canvas V" : "mirror canvas H";
}

static void perform_mirror_action(void)
{
    const char *status_text;

    status_text = mirror_status_text();
    if (!prepare_undo_step())
        return;
    busy_begin();
    set_canvas_dirty(true);
    canvas_modify_begin();
    mirror_canvas_region();
    canvas_modify_end();
    snapshot_refresh_current();
    busy_end();
    if (operation_was_cancelled())
        set_picker_status("cancelled");
    else
        set_picker_status(status_text);
}

static uint8_t selection_mode_active(void)
{
    return (active_tool == TOOL_SELECT || selection_active || selection_dragging) ? 1u : 0u;
}

static void wipe_canvas_region(void)
{
    uint8_t fill_color;

    fill_color = left_color ? BLACK : WHITE;
    operation_cancel_begin();
    if (selection_active)
    {
        fill_rect(selection_x1, selection_y1, selection_x2, selection_y2, fill_color);
        if (operation_was_cancelled())
            set_picker_status("cancelled");
        else
            set_picker_status("wipe selection");
        return;
    }
    if (selection_mode_active())
    {
        set_picker_status("NOSEL");
        return;
    }
    fill_canvas(BLACK, 0b00000000, 1u, 0u);
    if (operation_was_cancelled())
        set_picker_status("cancelled");
    else
        set_picker_status("wipe all");
}

static void save_canvas_bmp(void)
{
    int rc;

    if (!canvas_dirty)
        return;
    busy_begin();
    operation_cancel_begin();
    zoom_area_hide();
    if (zoom_view_active)
    {
        snapshot_load_canvas("TMP/paintHD_zoom.bin");
        zoom_view_active = false;
    }
    crosshair_hide();
    paste_preview_hide();
    primitive_hide_overlay();
    selection_hide_overlay();
    rc = SaveBMP(save_bmp_path, CANVAS_DATA, GFX_CANVAS_HEIGHT, GFX_CANVAS_WIDTH / 8);
    if (rc != 0)
    {
        paste_preview_show();
        primitive_show_overlay();
        selection_show_overlay();
        crosshair_show();
        busy_end();
        if (rc == -2)
        {
            set_picker_status("cancelled");
            return;
        }
        set_picker_status("SAVEERR");
        fprintf(stderr, "SaveBMP failed: %s\n", save_bmp_path);
    }
    else
    {
        paste_preview_show();
        primitive_show_overlay();
        selection_show_overlay();
        crosshair_show();
        busy_end();
        set_canvas_dirty(false);
        set_picker_status("SAVED");
    }
}

static void save_canvas_bmp_force(const char *path)
{
    int rc;

    busy_begin();
    operation_cancel_begin();
    zoom_area_hide();
    if (zoom_view_active)
    {
        snapshot_load_canvas("TMP/paintHD_zoom.bin");
        zoom_view_active = false;
    }
    crosshair_hide();
    paste_preview_hide();
    primitive_hide_overlay();
    selection_hide_overlay();
    rc = SaveBMP(path, CANVAS_DATA, GFX_CANVAS_HEIGHT, GFX_CANVAS_WIDTH / 8);
    if (rc != 0 && rc != -2)
        fprintf(stderr, "SaveBMP failed: %s\n", path);
    paste_preview_show();
    primitive_show_overlay();
    selection_show_overlay();
    crosshair_show();
    busy_end();
}

static void startup_after_click(void)
{
    static const char save_name[] = "paintHD_save.bmp";
    static const char new_name[]  = "paintHD_new.bmp";
    int fd;

    clear_selection();
    busy_begin();

    if (g_argc > 1)
    {
        if (LoadBMP(g_argv[1], CANVAS_DATA, GFX_CANVAS_HEIGHT, GFX_CANVAS_WIDTH / 8) != 0)
        {
            busy_end();
            set_picker_status("ERROR");
            return;
        }
        save_bmp_path = g_argv[1];
    }
    else
    {
        fd = open(save_name, O_RDONLY);
        if (fd >= 0)
        {
            close(fd);
            if (LoadBMP(save_name, CANVAS_DATA, GFX_CANVAS_HEIGHT, GFX_CANVAS_WIDTH / 8) != 0)
            {
                busy_end();
                set_picker_status("ERROR");
                return;
            }
            save_bmp_path = save_name;
            set_picker_status("RESTORED");
        }
        else
        {
            fill_canvas(BLACK, 0b00000000, 1u, 0u);
            save_bmp_path = new_name;
            save_canvas_bmp_force(new_name);
        }
    }

    set_canvas_dirty(false);
    snapshot_stack_clear('u', &undo_count);
    snapshot_stack_clear('r', &redo_count);
    snapshot_refresh_current();
    busy_end();
}

// Draw shape icon 8x8 pixels 1:1 at picker pixel (px, py); invert=1 for active state
static void draw_shape_icon(uint8_t shape, int px, int py, uint8_t invert, uint8_t fg_color)
{
    int row, col;
    uint8_t bits;
    uint8_t flipped;
    uint8_t pixel;
    uint8_t c;

    for (row = 0; row < 8; row++)
    {
        bits = icons[shape][row];
        if (shape == SHAPE_VERT && vert_brush_flipped)
        {
            if (row == 3 || row == 4)
                bits = 0xFFu;
            else
                bits = 0;
        }
        if (shape == SHAPE_DIAG && diag_brush_flipped)
        {
            flipped = 0;
            for (col = 0; col < 8; col++)
            {
                if (bits & (uint8_t)(0x80u >> col))
                    flipped |= (uint8_t)(1u << col);
            }
            bits = flipped;
        }
        for (col = 0; col < 8; col++)
        {
            pixel = (bits >> (7 - col)) & 1u;
            c = invert ? (pixel ? PICKER_PANEL_COLOR : fg_color)
                       : (pixel ? fg_color : PICKER_PANEL_COLOR);
            RIA.step0 = 0;
            RIA.addr0 = PICKER_DATA + PICKER_WIDTH * (py + row) + (px + col);
            RIA.rw0 = c;
        }
    }
}

static void draw_select_button(void)
{
    int row;
    int col;
    int x;
    int y;
    uint8_t active;
    uint16_t bits;
    uint8_t color;

    active = selection_mode_active();
    x = PICKER_SELECT_X;
    y = 2;
    draw_picker_box(active ? PICKER_FG_COLOR : PICKER_PANEL_COLOR,
                    x, 1, x + PICKER_BUTTON_SIZE - 1, 18);
    for (row = 0; row < 16; row++)
    {
        bits = select_icon[row];
        for (col = 0; col < 16; col++)
        {
            color = (bits & (uint16_t)(0x8000u >> col))
                        ? (active ? PICKER_PANEL_COLOR : ui_icon_color())
                        : (active ? PICKER_FG_COLOR : PICKER_PANEL_COLOR);
            RIA.step0 = 0;
            RIA.addr0 = PICKER_DATA + PICKER_WIDTH * (y + row) + (x + col);
            RIA.rw0 = color;
        }
    }
}

static void draw_rect_tool_button(void)
{
    int row;
    int col;
    int x;
    int y;
    uint8_t active;
    uint8_t enabled;
    uint16_t bits;
    uint8_t color;
    uint8_t icon_color;

    enabled = primitive_tools_enabled();
    active = enabled && (active_tool == TOOL_RECT);
    x = PICKER_RECT_X;
    y = 2;
    draw_picker_box(active ? PICKER_FG_COLOR : PICKER_PANEL_COLOR,
                    x, 1, x + PICKER_BUTTON_SIZE - 1, 18);
    icon_color = enabled ? ui_icon_color() : PICKER_SAVE_DISABLED_COLOR;
    for (row = 0; row < 16; row++)
    {
        bits = rect_tool_icon[row];
        for (col = 0; col < 16; col++)
        {
            color = (bits & (uint16_t)(0x8000u >> col))
                        ? (active ? PICKER_PANEL_COLOR : icon_color)
                        : (active ? PICKER_FG_COLOR : PICKER_PANEL_COLOR);
            RIA.step0 = 0;
            RIA.addr0 = PICKER_DATA + PICKER_WIDTH * (y + row) + (x + col);
            RIA.rw0 = color;
        }
    }
}

static void draw_ellipse_tool_button(void)
{
    int row;
    int col;
    int x;
    int y;
    uint8_t active;
    uint8_t enabled;
    uint16_t bits;
    uint8_t color;
    uint8_t icon_color;

    enabled = primitive_tools_enabled();
    active = enabled && (active_tool == TOOL_ELLIPSE);
    x = PICKER_ELLIPSE_X;
    y = 2;
    draw_picker_box(active ? PICKER_FG_COLOR : PICKER_PANEL_COLOR,
                    x, 1, x + PICKER_BUTTON_SIZE - 1, 18);
    icon_color = enabled ? ui_icon_color() : PICKER_SAVE_DISABLED_COLOR;
    for (row = 0; row < 16; row++)
    {
        bits = ellipse_tool_icon[row];
        for (col = 0; col < 16; col++)
        {
            color = (bits & (uint16_t)(0x8000u >> col))
                        ? (active ? PICKER_PANEL_COLOR : icon_color)
                        : (active ? PICKER_FG_COLOR : PICKER_PANEL_COLOR);
            RIA.step0 = 0;
            RIA.addr0 = PICKER_DATA + PICKER_WIDTH * (y + row) + (x + col);
            RIA.rw0 = color;
        }
    }
}

static void draw_shape_button(uint8_t shape)
{
    int px = PICKER_SHAPE0_X + shape * PICKER_BUTTON_SIZE;
    uint8_t enabled = brush_shape_enabled(shape);
    uint8_t icon_color = enabled ? ui_icon_color() : PICKER_SAVE_DISABLED_COLOR;
    uint8_t active = ((active_tool == TOOL_BRUSH ||
                       active_tool == TOOL_RECT ||
                       active_tool == TOOL_ELLIPSE) &&
                      brush_shape == shape &&
                      enabled);
    draw_picker_box(active ? PICKER_FG_COLOR : PICKER_PANEL_COLOR,
                    px, 1, px + PICKER_BUTTON_SIZE - 1, 18);
    draw_shape_icon(shape, px + 4, 6, active, icon_color);
}

static void draw_all_shape_buttons(void)
{
    uint8_t s;
    for (s = 0; s < SHAPE_COUNT; s++)
        draw_shape_button(s);
}

static void draw_picker_handle_only(void)
{
    int i, x, y;
    RIA.step0 = 1;
    for (y = 0; y < PICKER_HEIGHT; y++)
    {
        RIA.addr0 = PICKER_HANDLE_DATA + PICKER_COLLAPSED_WIDTH * y;
        for (x = 0; x < PICKER_COLLAPSED_WIDTH; x++)
            RIA.rw0 = PICKER_BG_COLOR;
    }
    for (y = 1; y <= 18; y++)
    {
        RIA.addr0 = PICKER_HANDLE_DATA + PICKER_COLLAPSED_WIDTH * y + PICKER_HANDLE_X;
        for (x = 0; x < PICKER_BUTTON_SIZE; x++)
            RIA.rw0 = PICKER_PANEL_COLOR;
    }
    for (i = 0; i < 4; i++)
    {
        y = 5 + i * 3;
        RIA.addr0 = PICKER_HANDLE_DATA + PICKER_COLLAPSED_WIDTH * y + PICKER_HANDLE_X + 3;
        for (x = PICKER_HANDLE_X + 3; x <= PICKER_HANDLE_X + PICKER_BUTTON_SIZE - 4; x++)
            RIA.rw0 = PICKER_FG_COLOR;
    }
}

static void draw_picker(void)
{
    int i;

    // main picker area
    draw_picker_box(PICKER_BG_COLOR, 0, 0, PICKER_WIDTH - 1, PICKER_HEIGHT - 1); // border

    draw_picker_box(PICKER_PANEL_COLOR, PICKER_HANDLE_X, 1, PICKER_HANDLE_X + PICKER_BUTTON_SIZE - 1, 18);
    for (i = 0; i < 4; i++)
        draw_picker_box(PICKER_FG_COLOR, PICKER_HANDLE_X + 3, 5 + (i * 3),
                        PICKER_HANDLE_X + PICKER_BUTTON_SIZE - 4, 5 + (i * 3));

    draw_picker_title();
    draw_save_button();
    draw_picker_box(PICKER_PANEL_COLOR, PICKER_SAVE_X + PICKER_BUTTON_SIZE, 1,
                    PICKER_SAVE_X + PICKER_BUTTON_SIZE + PICKER_SAVE_SPACER - 1, 18);
    draw_swap_button();
    draw_erase_button();
    draw_invert_button();
    draw_mirror_button();
    draw_picker_box(PICKER_PANEL_COLOR, PICKER_MIRROR_X + PICKER_BUTTON_SIZE, 1,
                    PICKER_MIRROR_X + PICKER_BUTTON_SIZE + PICKER_ERASE_SPACER - 1, 18);

    draw_select_button();
    draw_rect_tool_button();
    draw_ellipse_tool_button();
    draw_picker_box(PICKER_PANEL_COLOR, PICKER_ELLIPSE_X + PICKER_BUTTON_SIZE, 1,
                    PICKER_ELLIPSE_X + PICKER_BUTTON_SIZE + PICKER_PRIMITIVE_SPACER - 1, 18);
    draw_picker_text_button("-", PICKER_MINUS_X);
    draw_picker_text_button("+", PICKER_PLUS_X);

    draw_size_display();
    draw_all_shape_buttons();
    draw_mouse_coords(mouse_pos_x, mouse_pos_y);
    draw_picker_status();
}

static int picker_current_width(void)
{
    return picker_collapsed ? PICKER_COLLAPSED_WIDTH : PICKER_WIDTH;
}

static void move_picker(int x, int y)
{
    int pw = picker_current_width();
    picker_x = x;
    picker_y = y;
    if (picker_x < 0) picker_x = 0;
    if (picker_x > (int)GFX_CANVAS_WIDTH - pw)
        picker_x = (int)GFX_CANVAS_WIDTH - pw;
    if (picker_y < 0) picker_y = 0;
    if (picker_y > (int)GFX_CANVAS_HEIGHT - PICKER_HEIGHT)
        picker_y = (int)GFX_CANVAS_HEIGHT - PICKER_HEIGHT;
    xram0_struct_set(PICKER_STRUCT, vga_mode3_config_t, x_pos_px, picker_x);
    xram0_struct_set(PICKER_STRUCT, vga_mode3_config_t, y_pos_px, picker_y);
}

// Returns: -1=canvas, 2=drag, 3=erase, 4=minus, 5=plus, 6=swap, 7=save,
// 8=select, 9=size, 10-15=shape 0-5, 16=invert, 17=rect, 18=ellipse, 19=mirror, 99=none
static int picker_num(int x, int y)
{
    int shape;
    int pw = picker_current_width();
    x -= picker_x;
    y -= picker_y;
    if (x < 0 || x >= pw || y < 0 || y >= PICKER_HEIGHT)
        return -1;
    if (x < 1 || x >= pw - 1 || y < 1 || y >= PICKER_HEIGHT - 1)
        return 99; // border
    if (!picker_collapsed && x >= PICKER_COORDS_X)
        return 99;
    if (x >= PICKER_HANDLE_X && x < PICKER_HANDLE_X + PICKER_BUTTON_SIZE)
        return 2;
    if (x >= PICKER_SWAP_X && x < PICKER_SWAP_X + PICKER_BUTTON_SIZE)
        return 6;
    if (x >= PICKER_ERASE_X && x < PICKER_ERASE_X + PICKER_BUTTON_SIZE)
        return 3;
    if (x >= PICKER_INVERT_X && x < PICKER_INVERT_X + PICKER_BUTTON_SIZE)
        return 16;
    if (x >= PICKER_MIRROR_X && x < PICKER_MIRROR_X + PICKER_BUTTON_SIZE)
        return 19;
    if (x >= PICKER_SAVE_X && x < PICKER_SAVE_X + PICKER_BUTTON_SIZE)
    {
        if (!canvas_dirty)
            return 99;
        return 7;
    }
    if (x >= PICKER_SELECT_X && x < PICKER_SELECT_X + PICKER_BUTTON_SIZE)
        return 8;
    if (x >= PICKER_RECT_X && x < PICKER_RECT_X + PICKER_BUTTON_SIZE)
    {
        if (!primitive_tools_enabled())
            return 99;
        return 17;
    }
    if (x >= PICKER_ELLIPSE_X && x < PICKER_ELLIPSE_X + PICKER_BUTTON_SIZE)
    {
        if (!primitive_tools_enabled())
            return 99;
        return 18;
    }
    if (x >= PICKER_SHAPE0_X && x < PICKER_SHAPE0_X + SHAPE_COUNT * PICKER_BUTTON_SIZE)
    {
        shape = (x - PICKER_SHAPE0_X) / PICKER_BUTTON_SIZE;
        if (!brush_shape_enabled((uint8_t)shape))
            return 99;
        return 10 + shape;
    }
    if (x >= PICKER_MINUS_X && x < PICKER_MINUS_X + PICKER_BUTTON_SIZE)
        return 4;
    if (x >= PICKER_SIZE_X && x < PICKER_SIZE_X + PICKER_BUTTON_SIZE)
        return 9;
    if (x >= PICKER_PLUS_X && x < PICKER_PLUS_X + PICKER_BUTTON_SIZE)
        return 5;
    return 99;
}

static void swap_button_colors(void)
{
    uint8_t color;

    color = left_color;
    left_color = right_color;
    right_color = color;
    if (drawing_button == DRAW_BUTTON_LEFT)
        active_color = left_color;
    else if (drawing_button == DRAW_BUTTON_RIGHT)
        active_color = right_color;
    draw_swap_button();
    draw_erase_button();
}

static void change_brush_size(int delta)
{
    int s = (int)brush_size + delta;
    if (s < BRUSH_MIN) s = BRUSH_MIN;
    if (s > BRUSH_MAX) s = BRUSH_MAX;
    brush_size = (uint8_t)s;
    update_circle_cache();
    draw_size_display();
}

static void set_brush_size(uint8_t size)
{
    if (size < BRUSH_MIN) size = BRUSH_MIN;
    if (size > BRUSH_MAX) size = BRUSH_MAX;
    brush_size = size;
    update_circle_cache();
    draw_size_display();
}

static void handle_size_click(int click_count)
{
    if (click_count >= 3)
    {
        set_brush_size(BRUSH_MAX);
        set_picker_status("tool size max");
    }
    else if (click_count == 2)
    {
        set_brush_size(5u);
        set_picker_status("tool size 5px");
    }
    else if (click_count == 1)
    {
        set_brush_size(1u);
        set_picker_status("tool size 1px");
    }
}

static void change_active_tool(uint8_t tool)
{
    uint8_t old_shape;

    if ((tool == TOOL_RECT || tool == TOOL_ELLIPSE) && !primitive_tools_enabled())
        return;
    if (active_tool == tool)
        return;
    if (primitive_dragging)
        primitive_cancel();
    line_anchor_hide_marker();
    has_line_anchor = false;
    ctrl_line_session_active = false;
    old_shape = brush_shape;
    active_tool = tool;
    draw_select_button();
    draw_rect_tool_button();
    draw_ellipse_tool_button();
    draw_shape_button(old_shape);
    draw_shape_button(SHAPE_FILL);
    if (active_tool == TOOL_BRUSH)
        draw_shape_button(brush_shape);
    draw_erase_button();
    draw_invert_button();
    draw_mirror_button();
}

static void exit_selection_mode(void)
{
    clear_selection();
    change_active_tool(TOOL_BRUSH);
    drawing_button = DRAW_BUTTON_NONE;
    is_drawing = false;
    left_draw_armed = 0;
    right_draw_armed = 0;
    line_anchor_hide_marker();
    has_line_anchor = false;
    ctrl_line_session_active = false;
    draw_select_button();
    draw_shape_button(brush_shape);
    set_picker_status("selection cleared");
}

static void exit_primitive_mode(void)
{
    primitive_cancel();
    change_active_tool(TOOL_BRUSH);
    draw_rect_tool_button();
    draw_ellipse_tool_button();
    draw_shape_button(brush_shape);
}

static void change_brush_shape(uint8_t shape)
{
    uint8_t old = brush_shape;
    uint8_t keep_tool;

    keep_tool = active_tool;
    if (keep_tool == TOOL_SELECT)
        change_active_tool(TOOL_BRUSH);
    brush_shape = shape;
    if (brush_shape == SHAPE_FILL &&
        (active_tool == TOOL_RECT || active_tool == TOOL_ELLIPSE))
        change_active_tool(TOOL_BRUSH);
    draw_shape_button(old);
    draw_shape_button(brush_shape);
    draw_rect_tool_button();
    draw_ellipse_tool_button();
}

static void toggle_diag_brush_flip(void)
{
    diag_brush_flipped = !diag_brush_flipped;
    draw_shape_button(SHAPE_DIAG);
}

static void toggle_vert_brush_flip(void)
{
    vert_brush_flipped = !vert_brush_flipped;
    draw_shape_button(SHAPE_VERT);
}

static void canvas_modify_begin(void)
{
    zoom_area_hide();
    line_anchor_hide_marker();
    crosshair_hide();
    paste_preview_hide();
    primitive_hide_overlay();
    selection_hide_overlay();
}

static void canvas_modify_end(void)
{
    paste_preview_show();
    primitive_show_overlay();
    selection_show_overlay();
    crosshair_show();
}

static void drawing_session_begin(void)
{
    if (drawing_session_active)
        return;
    zoom_area_hide();
    line_anchor_hide_marker();
    crosshair_hide();
    paste_preview_hide();
    primitive_hide_overlay();
    selection_hide_overlay();
    drawing_session_active = true;
}

static void drawing_session_end(void)
{
    if (!drawing_session_active)
        return;
    drawing_session_active = false;
    if (undo_enabled)
    {
        busy_begin();
        snapshot_refresh_current();
        busy_end();
    }
    paste_preview_show();
    primitive_show_overlay();
    selection_show_overlay();
    crosshair_show();
}

static void left_press(int x, int y)
{
    static clock_t last_drag_click;
    static clock_t last_vert_click;
    static clock_t last_diag_click;
    int num = picker_num(x, y);
    clock_t now;

    if (num != 19 && mirror_click_pending != 0)
    {
        mirror_click_pending = 0;
        mirror_click_deadline = 0;
    }

    if (zoom_area_active)
    {
        if (num < 0)
        {
            zoom_area_hide();
            zoom_area_active = false;
            busy_begin();
            snapshot_save_canvas("TMP/paintHD_zoom.bin");
            zoom_area_pixels_save();
            fill_canvas(BLACK, 0b10101010, 4u, 1u);
            zoom_draw_view();
            busy_end();
            zoom_enter_view();
            picker_hover_status = 0;
            set_picker_hover_status("ZOOM");
        }
        return;
    }

    if (zoom_view_active)
    {
        if (num < 0)
        {
            int bx = x - (int)ZOOM_VIEW_X0;
            int by = y - (int)ZOOM_VIEW_Y0;
            if (bx >= 0 && bx < (int)ZOOM_VIEW_W && by >= 0 && by < (int)ZOOM_VIEW_H)
            {
                int sx = bx / ZOOM_DOT;
                int sy = by / ZOOM_DOT;
                RIA.addr1 = ZOOM_BUF_ADDR + (unsigned)sy * ZOOM_AREA + (unsigned)sx;
                RIA.step1 = 0;
                zoom_paint_value = RIA.rw1 ? 0 : 1;
                RIA.addr1 = ZOOM_BUF_ADDR + (unsigned)sy * ZOOM_AREA + (unsigned)sx;
                RIA.step1 = 0;
                RIA.rw1 = zoom_paint_value;
                zoom_redraw_block(sx, sy);
                zoom_paint_armed = 1;
                zoom_paint_last_sx = sx;
                zoom_paint_last_sy = sy;
            }
            return;
        }
    }
    else

    if (paste_preview_active)
    {
        if (num < 0)
        {
            paste_preview_move(x, y);
            clipboard_paste_apply();
            return;
        }
        paste_preview_cancel();
    }

    if (help_pending)
    {
        help_pending = 0;
        busy_begin();
        LoadBMP("paintHD_tmp.bmp", CANVAS_DATA, GFX_CANVAS_HEIGHT, GFX_CANVAS_WIDTH / 8);
        busy_end();
        return;
    }

    if (startup_splash_pending)
    {
        startup_splash_pending = 0;
        startup_after_click();
        return;
    }

    if (canvas_input_locked())
    {
        if (num < 0 || num == 3)
            return;
    }

    if (num < 0)
    {
        last_drag_click = 0;
        left_draw_armed = 0;
        if (active_tool == TOOL_SELECT)
        {
            line_anchor_hide_marker();
            has_line_anchor = false;
            ctrl_line_session_active = false;
            selection_begin_drag(x, y);
            return;
        }
        if (active_tool == TOOL_RECT || active_tool == TOOL_ELLIPSE)
        {
            line_anchor_hide_marker();
            has_line_anchor = false;
            ctrl_line_session_active = false;
            primitive_begin_drag(active_tool, DRAW_BUTTON_LEFT, left_color, x, y);
            return;
        }
        if (key_pressed(HID_LEFT_CTRL) && brush_shape != SHAPE_FILL)
        {
            if (!has_line_anchor)
            {
                has_line_anchor = true;
                line_anchor_x = x;
                line_anchor_y = y;
                line_anchor_show_marker();
                set_picker_hover_status("set end point");
                return;
            }
            active_color = left_color;
            if (shift_pressed())
                constrain_line_axis(line_anchor_x, line_anchor_y, &x, &y);
            if (!ctrl_line_session_active)
            {
                if (!prepare_undo_step())
                    return;
                ctrl_line_session_active = true;
            }
            busy_begin();
            set_canvas_dirty(true);
            canvas_modify_begin();
            draw_line_brush(line_anchor_x, line_anchor_y, x, y);
            canvas_modify_end();
            busy_end();
            set_picker_status("drawing line");
            line_anchor_x = x;
            line_anchor_y = y;
            line_anchor_show_marker();
            return;
        }
        line_anchor_hide_marker();
        has_line_anchor = false;
        ctrl_line_session_active = false;
        if (brush_shape == SHAPE_FILL)
        {
            if (!prepare_undo_step())
                return;
            busy_begin();
            set_canvas_dirty(true);
            canvas_modify_begin();
            flood_fill(x, y, left_color ? 0 : 1, left_color);
            canvas_modify_end();
            snapshot_refresh_current();
            busy_end();
            return;
        }
        left_draw_armed = 1;
        if (!prepare_undo_step())
        {
            left_draw_armed = 0;
            return;
        }
        drawing_session_begin();
        start_canvas_draw(DRAW_BUTTON_LEFT, left_color, x, y);
        return;
    }
    if (num == 2)
    {
        now = clock();
        if (last_drag_click != 0 &&
            (clock_t)(now - last_drag_click) <= (clock_t)DOUBLE_CLICK_TICKS)
        {
            move_picker(0, 0);
            is_dragging = false;
            last_drag_click = 0;
        }
        else
        {
            is_dragging = true;
            drag_x = x - picker_x;
            drag_y = y - picker_y;
            last_drag_click = now;
        }
    }
    else
    {
        last_drag_click = 0;
        line_anchor_hide_marker();
        has_line_anchor = false;
        ctrl_line_session_active = false;
    }
    if (zoom_view_active)
        return;
    if (num == 9)
    {
        now = clock();
        if (size_click_pending != 0 &&
            size_click_deadline != 0 &&
            now <= size_click_deadline)
        {
            size_click_pending++;
            size_click_deadline = now + (clock_t)DOUBLE_CLICK_TICKS;
            if (size_click_pending >= 3u)
            {
                handle_size_click(3);
                size_click_pending = 0;
                size_click_deadline = 0;
            }
        }
        else
        {
            size_click_pending = 1u;
            size_click_deadline = now + (clock_t)DOUBLE_CLICK_TICKS;
        }
    }
    if (num == 10 + SHAPE_VERT)
    {
        now = clock();
        if (last_vert_click != 0 &&
            (clock_t)(now - last_vert_click) <= (clock_t)DOUBLE_CLICK_TICKS)
        {
            toggle_vert_brush_flip();
            last_vert_click = 0;
        }
        else
        {
            last_vert_click = now;
        }
    }
    else
    {
        last_vert_click = 0;
    }
    if (num == 10 + SHAPE_DIAG)
    {
        now = clock();
        if (last_diag_click != 0 &&
            (clock_t)(now - last_diag_click) <= (clock_t)DOUBLE_CLICK_TICKS)
        {
            toggle_diag_brush_flip();
            last_diag_click = 0;
        }
        else
        {
            last_diag_click = now;
        }
    }
    else
    {
        last_diag_click = 0;
    }
    if (num == 19)
    {
        now = clock();
        if (mirror_click_pending != 0 &&
            mirror_click_deadline != 0 &&
            now <= mirror_click_deadline)
        {
            toggle_mirror_mode();
            mirror_click_pending = 0;
            mirror_click_deadline = 0;
        }
        else
        {
            mirror_click_pending = 1u;
            mirror_click_deadline = now + (clock_t)DOUBLE_CLICK_TICKS;
        }
        return;
    }
    mirror_click_pending = 0;
    mirror_click_deadline = 0;
    if (num == 6)
        swap_button_colors();
    if (num == 8)
    {
        if (active_tool == TOOL_SELECT)
            exit_selection_mode();
        else
            change_active_tool(TOOL_SELECT);
    }
    if (num == 17)
    {
        if (active_tool == TOOL_RECT)
            exit_primitive_mode();
        else
            change_active_tool(TOOL_RECT);
    }
    if (num == 18)
    {
        if (active_tool == TOOL_ELLIPSE)
            exit_primitive_mode();
        else
            change_active_tool(TOOL_ELLIPSE);
    }
    if (num == 3)
    {
        if (selection_mode_active() && !selection_active)
        {
            wipe_canvas_region();
            return;
        }
        if (!prepare_undo_step())
            return;
        busy_begin();
        canvas_modify_begin();
        wipe_canvas_region();
        canvas_modify_end();
        snapshot_refresh_current();
        busy_end();
        if (!(selection_mode_active() && !selection_active))
            set_canvas_dirty(true);
    }
    if (num == 16)
    {
        const char *status_text;

        status_text = selection_active ? "invert selection" : "invert canvas";
        if (!prepare_undo_step())
            return;
        busy_begin();
        set_canvas_dirty(true);
        canvas_modify_begin();
        invert_canvas_region();
        canvas_modify_end();
        snapshot_refresh_current();
        busy_end();
        if (operation_was_cancelled())
            set_picker_status("cancelled");
        else
            set_picker_status(status_text);
    }
    if (num == 7)
        save_canvas_bmp();
    if (num == 4)
        change_brush_size(-1);
    if (num == 5)
        change_brush_size(1);
    if (num >= 10 && num < 10 + SHAPE_COUNT)
        change_brush_shape((uint8_t)(num - 10));
}

static void left_release(void)
{
    zoom_paint_armed = 0;
    if (selection_dragging)
        selection_finish_drag();
    if (primitive_dragging && drawing_button == DRAW_BUTTON_LEFT)
        primitive_finish_drag();
    left_draw_armed = 0;
    if (drawing_button == DRAW_BUTTON_LEFT)
    {
        if (right_draw_armed)
        {
            drawing_button = DRAW_BUTTON_RIGHT;
            active_color = right_color;
        }
        else
        {
            if (is_drawing)
            {
                has_line_anchor = true;
                ctrl_line_session_active = false;
                line_anchor_x = line_x;
                line_anchor_y = line_y;
            }
            drawing_button = DRAW_BUTTON_NONE;
            is_drawing = false;
            drawing_session_end();
        }
    }
    is_dragging = false;
}

static void ctrl_right_press(int x, int y)
{
    active_color = right_color;
    if (shift_pressed())
        constrain_line_axis(line_anchor_x, line_anchor_y, &x, &y);
    if (!ctrl_line_session_active)
    {
        if (!prepare_undo_step())
            return;
        ctrl_line_session_active = true;
    }
    busy_begin();
    set_canvas_dirty(true);
    canvas_modify_begin();
    draw_line_brush(line_anchor_x, line_anchor_y, x, y);
    canvas_modify_end();
    busy_end();
    set_picker_status("drawing line");
    line_anchor_x = x;
    line_anchor_y = y;
    line_anchor_show_marker();
}

static void right_press(int x, int y)
{
    int num = picker_num(x, y);

    if (num != 19 && mirror_click_pending != 0)
    {
        mirror_click_pending = 0;
        mirror_click_deadline = 0;
    }

    zoom_cancel();

    if (paste_preview_active)
    {
        paste_preview_cancel();
        return;
    }

    if (canvas_input_locked())
    {
        if (num < 0 || num == 3)
            return;
    }

    if (key_pressed(HID_LEFT_CTRL) && brush_shape != SHAPE_FILL &&
        has_line_anchor && num < 0 && !canvas_input_locked())
    {
        ctrl_right_press(x, y);
        return;
    }
    line_anchor_hide_marker();
    has_line_anchor = false;
    ctrl_line_session_active = false;
    if (num < 0)
    {
        if (active_tool == TOOL_SELECT)
            return;
        if (active_tool == TOOL_RECT || active_tool == TOOL_ELLIPSE)
        {
            primitive_begin_drag(active_tool, DRAW_BUTTON_RIGHT, right_color, x, y);
            return;
        }
        right_draw_armed = 0;
        if (brush_shape == SHAPE_FILL)
        {
            if (!prepare_undo_step())
                return;
            busy_begin();
            set_canvas_dirty(true);
            canvas_modify_begin();
            flood_fill(x, y, right_color ? 0 : 1, right_color);
            canvas_modify_end();
            snapshot_refresh_current();
            busy_end();
            return;
        }
        right_draw_armed = 1;
        if (!prepare_undo_step())
        {
            right_draw_armed = 0;
            return;
        }
        drawing_session_begin();
        start_canvas_draw(DRAW_BUTTON_RIGHT, right_color, x, y);
        return;
    }
    if (num == 6)
        swap_button_colors();
    if (num == 8)
    {
        if (active_tool == TOOL_SELECT)
            exit_selection_mode();
        else
            change_active_tool(TOOL_SELECT);
    }
    if (num == 17)
    {
        if (active_tool == TOOL_RECT)
            exit_primitive_mode();
        else
            change_active_tool(TOOL_RECT);
    }
    if (num == 18)
    {
        if (active_tool == TOOL_ELLIPSE)
            exit_primitive_mode();
        else
            change_active_tool(TOOL_ELLIPSE);
    }
    if (num == 3)
    {
        if (selection_mode_active() && !selection_active)
        {
            wipe_canvas_region();
            return;
        }
        if (!prepare_undo_step())
            return;
        busy_begin();
        canvas_modify_begin();
        wipe_canvas_region();
        canvas_modify_end();
        snapshot_refresh_current();
        busy_end();
        if (!(selection_mode_active() && !selection_active))
            set_canvas_dirty(true);
    }
    if (num == 16)
    {
        const char *status_text;

        status_text = selection_active ? "invert selection" : "invert canvas";
        if (!prepare_undo_step())
            return;
        busy_begin();
        set_canvas_dirty(true);
        canvas_modify_begin();
        invert_canvas_region();
        canvas_modify_end();
        snapshot_refresh_current();
        busy_end();
        if (operation_was_cancelled())
            set_picker_status("cancelled");
        else
            set_picker_status(status_text);
    }
    if (num == 19)
    {
        const char *status_text;

        status_text = mirror_status_text();
        if (!prepare_undo_step())
            return;
        busy_begin();
        set_canvas_dirty(true);
        canvas_modify_begin();
        mirror_canvas_region();
        canvas_modify_end();
        snapshot_refresh_current();
        busy_end();
        if (operation_was_cancelled())
            set_picker_status("cancelled");
        else
            set_picker_status(status_text);
    }
    if (num == 7)
        save_canvas_bmp();
    if (num == 4)
        change_brush_size(-1);
    if (num == 5)
        change_brush_size(1);
    if (num >= 10 && num < 10 + SHAPE_COUNT)
        change_brush_shape((uint8_t)(num - 10));
    if (num == 2)
    {
        picker_collapsed = picker_collapsed ? 0u : 1u;
        if (picker_collapsed)
        {
            draw_picker_handle_only();
            xram0_struct_set(PICKER_STRUCT, vga_mode3_config_t, xram_data_ptr, PICKER_HANDLE_DATA);
            xram0_struct_set(PICKER_STRUCT, vga_mode3_config_t, width_px, PICKER_COLLAPSED_WIDTH);
        }
        else
        {
            xram0_struct_set(PICKER_STRUCT, vga_mode3_config_t, xram_data_ptr, PICKER_DATA);
            xram0_struct_set(PICKER_STRUCT, vga_mode3_config_t, width_px, PICKER_WIDTH);
        }
        move_picker(picker_x, picker_y);
    }
}

static void right_release(void)
{
    if (primitive_dragging && drawing_button == DRAW_BUTTON_RIGHT)
        primitive_finish_drag();
    right_draw_armed = 0;
    if (drawing_button == DRAW_BUTTON_RIGHT)
    {
        if (left_draw_armed)
        {
            drawing_button = DRAW_BUTTON_LEFT;
            active_color = left_color;
        }
        else
        {
            drawing_button = DRAW_BUTTON_NONE;
            is_drawing = false;
            drawing_session_end();
        }
    }
}

static void move(int x, int y)
{
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
                RIA.addr1 = ZOOM_BUF_ADDR + (unsigned)sy * ZOOM_AREA + (unsigned)sx;
                RIA.step1 = 0;
                RIA.rw1 = zoom_paint_value;
                zoom_redraw_block(sx, sy);
                zoom_paint_last_sx = sx;
                zoom_paint_last_sy = sy;
            }
        }
        return;
    }
    if (is_dragging)
    {
        move_picker(x - drag_x, y - drag_y);
    }
    else if (paste_preview_active)
    {
        paste_preview_move(x, y);
    }
    else if (selection_dragging)
    {
        selection_update_drag(x, y);
    }
    else if (primitive_dragging)
    {
        primitive_update_drag(x, y);
    }
    else if (canvas_input_locked())
    {
        return;
    }
    else if (is_drawing)
    {
        if (brush_shape == SHAPE_SPRAY)
        {
            set_canvas_dirty(true);
            draw_brush(x, y);
        }
        else
        {
            set_canvas_dirty(true);
            draw_freehand_brush(line_x, line_y, x, y);
            line_x = x;
            line_y = y;
        }
    }
}

#ifndef XRAM_POINTERS

static void draw_pointer(uint8_t type)
{
    // clang-format on
    int i;
    RIA.addr0 = POINTER_DATA;
    RIA.step0 = 1;
    switch(type){
    case 0:
        for (i = 0; i < sizeof(arrow); i++) RIA.rw0 = arrow[i];
        break;
    case 1:
        for (i = 0; i < sizeof(cross); i++) RIA.rw0 = cross[i];
        break;
    case 2:
        for (i = 0; i < sizeof(hourglass); i++) RIA.rw0 = hourglass[i];
    }
}

#else

static void draw_pointer(uint8_t type)
{
    switch(type){
    case 0:
        xram0_struct_set(POINTER_STRUCT, vga_mode3_config_t, xram_data_ptr, XRAM_POINTERS_arrow);
        break;
    case 1:
        xram0_struct_set(POINTER_STRUCT, vga_mode3_config_t, xram_data_ptr, XRAM_POINTERS_cross);
        break;
    case 2:
        xram0_struct_set(POINTER_STRUCT, vga_mode3_config_t, xram_data_ptr, XRAM_POINTERS_hourglass);
    }
}

#endif

static void mouse(void)
{
    static uint8_t mb;
    static uint8_t prev_wheel;
    static int prev_display_x = -1;
    static int prev_display_y = -1;
    clock_t now;
    int x, y;
    uint8_t rw, changed, pressed, released;
    uint8_t cursor;

    SEI();
    x = mouse_pos_x;
    y = mouse_pos_y;
    CLI();

    RIA.addr0 = MOUSE_INPUT_BUTTONS;
    rw = RIA.rw0;
    changed = mb ^ rw;
    pressed = rw & changed;
    released = mb & changed;
    mb = rw;
    if (pressed & 1)  left_press(x, y);
    if (released & 1) left_release();
    if (!zoom_view_active)
    {
        if (pressed & 2)  right_press(x, y);
        if (released & 2) right_release();
    }
    if (!zoom_view_active && (pressed & 4))
    {
        zoom_cancel();
        if (crosshair_active)
        {
            crosshair_hide();
            crosshair_active = false;
        }
        else
        {
            crosshair_active = true;
            crosshair_show();
        }
    }

    RIA.addr0 = MOUSE_INPUT_WHEEL;
    rw = RIA.rw0;
    if (rw != prev_wheel)
    {
        if (!zoom_view_active)
        {
            zoom_cancel();
            change_brush_size((int8_t)(rw - prev_wheel));
        }
        prev_wheel = rw;
    }

    now = clock();
    if (!busy_mode &&
        picker_status_deadline != 0 &&
        now >= picker_status_deadline &&
        picker_status[0] != '\0')
    {
        picker_status[0] = '\0';
        picker_status_deadline = 0;
        draw_picker_status();
    }
    if (size_click_pending != 0 &&
        size_click_deadline != 0 &&
        now > size_click_deadline)
    {
        handle_size_click(size_click_pending);
        size_click_pending = 0;
        size_click_deadline = 0;
    }
    if (mirror_click_pending != 0 &&
        mirror_click_deadline != 0 &&
        now > mirror_click_deadline)
    {
        perform_mirror_action();
        mirror_click_pending = 0;
        mirror_click_deadline = 0;
    }

    if (x != prev_display_x || y != prev_display_y)
    {
        if (zoom_area_active)
            zoom_area_move(x, y);
        if (crosshair_visible)
        {
            crosshair_hide();
            crosshair_show();
        }
        draw_mouse_coords(x, y);
        prev_display_x = x;
        prev_display_y = y;
    }

    if (zoom_area_active)
    {
        set_picker_hover_status("set area to zoom");
    }
    else if (zoom_view_active)
    {
        set_picker_hover_status("ZOOM");
    }
    else if (!canvas_input_locked() && !paste_preview_active &&
        active_tool == TOOL_BRUSH && brush_shape != SHAPE_FILL &&
        picker_num(x, y) < 0 && key_pressed(HID_LEFT_CTRL) &&
        !alt_pressed() &&
        !key_pressed(HID_A) && !key_pressed(HID_C) && !key_pressed(HID_V) &&
        !key_pressed(HID_X) && !key_pressed(HID_Y) && !key_pressed(HID_Z))
    {
        ctrl_hover_deadline = 0;
        if (has_line_anchor)
            set_picker_hover_status("set end point");
        else
            set_picker_hover_status("set start point");
    }
    else
    {
        ctrl_hover_deadline = 0;
        line_anchor_hide_marker();
        if (ctrl_line_session_active)
        {
            ctrl_line_session_active = false;
            has_line_anchor = false;
            busy_begin();
            snapshot_refresh_current();
            busy_end();
        }
        else
            has_line_anchor = false;
        set_picker_hover_status(picker_hover_text(x, y));
    }

    move(x, y);

    if (!busy_mode)
    {
        cursor = (picker_num(x, y) >= 0) ? 0 : 1;
        if (cursor != current_cursor)
        {
            current_cursor = cursor;
            draw_pointer(cursor);
        }
    }
}

int main(int argc, char *argv[]){
    
    static const char default_save_name[] = "paintHD_new.bmp";
    static uint8_t prev_ctrl_a;
    static uint8_t prev_ctrl_c;
    static uint8_t prev_ctrl_q;
    static uint8_t prev_ctrl_s;
    static uint8_t prev_ctrl_x;
    static uint8_t prev_ctrl_v;
    static uint8_t prev_ctrl_y;
    static uint8_t prev_ctrl_z;
    static uint8_t prev_ctrl_alt_v;
    static uint8_t prev_ctrl_m;
    static uint8_t prev_ctrl_shift_alt_t;
    static uint8_t prev_escape;
    static uint8_t prev_enter;
    static uint8_t prev_f1;
    static uint8_t prev_f2;
    static uint8_t prev_f3;

    #ifdef DEBUG
    {
        int i;
            
        printf("argc = %d\n", argc);
        for (i = 0; i < argc; i++)
            printf("argv[%d] = %s\n", i, argv[i]);

        if (argc == 2)
        {
            printf("Success\n");
            return 0;
        }
    //  exit(0);
    //  return 0;
    }
    #endif

    xreg_vga_canvas(GFX_CANVAS_TYPE);

    xram0_struct_set(CANVAS_STRUCT, vga_mode3_config_t, x_wrap, false);
    xram0_struct_set(CANVAS_STRUCT, vga_mode3_config_t, y_wrap, false);
    xram0_struct_set(CANVAS_STRUCT, vga_mode3_config_t, x_pos_px, 0);
    xram0_struct_set(CANVAS_STRUCT, vga_mode3_config_t, y_pos_px, 0);
    xram0_struct_set(CANVAS_STRUCT, vga_mode3_config_t, width_px, GFX_CANVAS_WIDTH);
    xram0_struct_set(CANVAS_STRUCT, vga_mode3_config_t, height_px, GFX_CANVAS_HEIGHT);
    xram0_struct_set(CANVAS_STRUCT, vga_mode3_config_t, xram_data_ptr, CANVAS_DATA);
    xram0_struct_set(CANVAS_STRUCT, vga_mode3_config_t, xram_palette_ptr, 0xFFFF);

    xram0_struct_set(PICKER_STRUCT, vga_mode3_config_t, x_wrap, false);
    xram0_struct_set(PICKER_STRUCT, vga_mode3_config_t, y_wrap, false);
    xram0_struct_set(PICKER_STRUCT, vga_mode3_config_t, width_px, PICKER_WIDTH);
    xram0_struct_set(PICKER_STRUCT, vga_mode3_config_t, height_px, PICKER_HEIGHT);
    xram0_struct_set(PICKER_STRUCT, vga_mode3_config_t, xram_data_ptr, PICKER_DATA);
    xram0_struct_set(PICKER_STRUCT, vga_mode3_config_t, xram_palette_ptr, 0xFFFF);

    xram0_struct_set(POINTER_STRUCT, vga_mode3_config_t, x_wrap, false);
    xram0_struct_set(POINTER_STRUCT, vga_mode3_config_t, y_wrap, false);
    xram0_struct_set(POINTER_STRUCT, vga_mode3_config_t, width_px, POINTER_WIDTH);
    xram0_struct_set(POINTER_STRUCT, vga_mode3_config_t, height_px, POINTER_HEIGHT);
    xram0_struct_set(POINTER_STRUCT, vga_mode3_config_t, xram_data_ptr, POINTER_DATA);
    xram0_struct_set(POINTER_STRUCT, vga_mode3_config_t, xram_palette_ptr, 0xFFFF);

    g_argc = argc;
    g_argv = argv;

    brush_size = 5;
    brush_shape = SHAPE_SQUARE;
    active_tool = TOOL_BRUSH;
    vert_brush_flipped = false;
    diag_brush_flipped = false;
    mirror_vertical = true;
    left_color = 1;
    right_color = 0;
    drawing_button = DRAW_BUTTON_NONE;
    left_draw_armed = 0;
    right_draw_armed = 0;
    has_line_anchor = false;
    ctrl_line_session_active = false;
    canvas_dirty = false;
    drawing_session_active = false;
    picker_status[0] = '\0';
    picker_hover_status = 0;
    picker_status_deadline = 0;
    startup_splash_pending = 0;
    size_click_deadline = 0;
    size_click_pending = 0;
    mirror_click_deadline = 0;
    mirror_click_pending = 0;
    ctrl_hover_deadline = 0;
    selection_active = false;
    selection_dragging = false;
    selection_overlay_visible = false;
    selection_x1 = 1;
    selection_y1 = 1;
    selection_x2 = 0;
    selection_y2 = 0;
    primitive_dragging = false;
    primitive_overlay_visible = false;
    primitive_x1 = 1;
    primitive_y1 = 1;
    primitive_x2 = 0;
    primitive_y2 = 0;
    prev_ctrl_a = 0;
    prev_ctrl_c = 0;
    prev_ctrl_q = 0;
    prev_ctrl_s = 0;
    prev_ctrl_x = 0;
    prev_ctrl_v = 0;
    prev_ctrl_y = 0;
    prev_ctrl_z = 0;
    prev_ctrl_alt_v = 0;
    prev_escape = 0;
    prev_enter = 0;
    prev_f2 = 0;
    prev_f3 = 0;
    clipboard_valid = 0;
    clipboard_width = 0;
    clipboard_height = 0;
    clipboard_stride = 0;
    paste_preview_active = false;
    paste_preview_visible = false;
    paste_transparent = false;
    current_snapshot_valid = 0u;
    undo_count = 0;
    redo_count = 0;
    memset(undo_dirty_stack, 0, sizeof(undo_dirty_stack));
    memset(redo_dirty_stack, 0, sizeof(redo_dirty_stack));
    snapshot_stack_clear('u', &undo_count);
    snapshot_stack_clear('r', &redo_count);

    f_mkdir("TMP");

    xreg_vga_mode(GFX_MODE_BITMAP, GFX_BITMAP_bpp1, CANVAS_STRUCT, GFX_PLANE_0);
    #ifdef RELEASE
    PAUSE(200);
    clear_canvas_random_blocks8();
    #endif
    snapshot_refresh_current();
    draw_picker();
    move_picker(((GFX_CANVAS_WIDTH - (PICKER_WIDTH))/2), GFX_CANVAS_HEIGHT - (PICKER_HEIGHT * 2));
    draw_pointer(POINTER_arrow);

    LoadBMP("ROM:paintHDhelp.bmp", CANVAS_DATA, GFX_CANVAS_HEIGHT, GFX_CANVAS_WIDTH / 8);
    startup_splash_pending = 1u;

    xreg_vga_mode(GFX_MODE_BITMAP, GFX_BITMAP_bpp8, PICKER_STRUCT, GFX_PLANE_1);
    xreg_vga_mode(GFX_MODE_BITMAP, GFX_BITMAP_bpp8, POINTER_STRUCT, GFX_PLANE_2);

    xreg_ria_keyboard(KEYBOARD_INPUT);
    mouse_init();
    while (1)
    {
        if (key_pressed(HID_LEFT_CTRL) && key_pressed(HID_A))
        {
            if (!prev_ctrl_a)
            {
                if (active_tool == TOOL_SELECT || selection_active || selection_dragging)
                    exit_selection_mode();
                prev_ctrl_a = 1u;
            }
        }
        else
        {
            prev_ctrl_a = 0;
        }
        if (zoom_view_active && key_pressed(HID_ENTER))
        {
            if (!prev_enter)
            {
                prev_enter = 1u;
                canvas_modify_begin();
                busy_begin();
                zoom_apply_changes();
                busy_end();
                canvas_modify_end();
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
                if (zoom_area_active)
                {
                    zoom_area_hide();
                    zoom_area_active = false;
                    set_picker_status("");
                }
                else if (zoom_view_active)
                {
                    busy_begin();
                    snapshot_load_canvas("TMP/paintHD_zoom.bin");
                    busy_end();
                    zoom_exit_view();
                    set_picker_status("");
                }
                else if (paste_preview_active)
                    paste_preview_cancel();
                else if (primitive_mode_active())
                    exit_primitive_mode();
                else if (active_tool == TOOL_SELECT || selection_active || selection_dragging)
                    exit_selection_mode();
                prev_escape = 1u;
            }
        }
        else
        {
            prev_escape = 0;
        }
        if (key_pressed(HID_F1))
        {
            if (!zoom_view_active && !prev_f1)
            {
                save_canvas_bmp_force("paintHD_tmp.bmp");
                LoadBMP("ROM:paintHDhelp.bmp", CANVAS_DATA, GFX_CANVAS_HEIGHT, GFX_CANVAS_WIDTH / 8);
                help_pending = 1u;
                prev_f1 = 1u;
            }
        }
        else
        {
            prev_f1 = 0;
        }
        if (key_pressed(HID_F2))
        {
            if (!zoom_view_active && !prev_f2)
            {
                undo_enabled = undo_enabled ? 0u : 1u;
                set_picker_status(undo_enabled ? "undo ON" : "undo OFF");
                prev_f2 = 1u;
            }
        }
        else
        {
            prev_f2 = 0;
        }
        if (key_pressed(HID_F3))
        {
            if (!zoom_view_active && !prev_f3)
            {
                busy_begin();
                operation_cancel_begin();
                if (redo_count != 0u)
                    snapshot_stack_clear('r', &redo_count);
                if (snapshot_stage_current('u', undo_dirty_stack, &undo_count,
                                           canvas_dirty ? 1u : 0u))
                {
                    if (snapshot_save_canvas(current_snapshot_path))
                        current_snapshot_valid = 1u;
                }
                busy_end();
                set_picker_status("snapshot saved");
                prev_f3 = 1u;
            }
        }
        else
        {
            prev_f3 = 0;
        }
        if (!canvas_input_locked() && !paste_preview_active &&
            key_pressed(HID_LEFT_CTRL) && !alt_pressed() && key_pressed(HID_Z))
        {
            if (!prev_ctrl_z)
            {
                perform_undo();
                prev_ctrl_z = 1u;
            }
        }
        else
        {
            prev_ctrl_z = 0;
        }
        if (!canvas_input_locked() && !paste_preview_active &&
            key_pressed(HID_LEFT_CTRL) && !alt_pressed() && key_pressed(HID_Y))
        {
            if (!prev_ctrl_y)
            {
                perform_redo();
                prev_ctrl_y = 1u;
            }
        }
        else
        {
            prev_ctrl_y = 0;
        }
        if (!canvas_input_locked() && !paste_preview_active &&
            key_pressed(HID_LEFT_CTRL) && shift_pressed() && key_pressed(HID_V))
        {
            if (!prev_ctrl_alt_v)
            {
                paste_preview_begin(1u);
                prev_ctrl_alt_v = 1u;
            }
        }
        else
        {
            prev_ctrl_alt_v = 0;
        }
        if (key_pressed(HID_LEFT_CTRL) && !alt_pressed() && !shift_pressed() &&
            key_pressed(HID_S))
        {
            if (!prev_ctrl_s)
            {
                save_canvas_bmp();
                prev_ctrl_s = 1u;
            }
        }
        else
        {
            prev_ctrl_s = 0;
        }
        if (key_pressed(HID_LEFT_CTRL) && !alt_pressed() && !shift_pressed() &&
            key_pressed(HID_Q))
        {
            if (!prev_ctrl_q)
            {
                save_canvas_bmp_force("paintHD_save.bmp");
                return 0;
            }
        }
        else
        {
            prev_ctrl_q = 0;
        }
        if (!canvas_input_locked() && !paste_preview_active &&
            key_pressed(HID_LEFT_CTRL) && !alt_pressed() && key_pressed(HID_C))
        {
            if (!prev_ctrl_c)
            {
                clipboard_copy_selection();
                prev_ctrl_c = 1u;
            }
        }
        else
        {
            prev_ctrl_c = 0;
        }
        if (!canvas_input_locked() && !paste_preview_active &&
            key_pressed(HID_LEFT_CTRL) && !alt_pressed() && key_pressed(HID_X))
        {
            if (!prev_ctrl_x)
            {
                clipboard_cut_selection();
                prev_ctrl_x = 1u;
            }
        }
        else
        {
            prev_ctrl_x = 0;
        }
        if (!canvas_input_locked() && !paste_preview_active &&
            key_pressed(HID_LEFT_CTRL) && !alt_pressed() && key_pressed(HID_V))
        {
            if (!prev_ctrl_v)
            {
                paste_preview_begin(0u);
                prev_ctrl_v = 1u;
            }
        }
        else
        {
            prev_ctrl_v = 0;
        }
        if (!canvas_input_locked() && !paste_preview_active && !zoom_view_active &&
            key_pressed(HID_LEFT_CTRL) && !alt_pressed() && key_pressed(HID_M))
        {
            if (!prev_ctrl_m)
            {
                if (!zoom_area_active && prepare_undo_step())
                {
                    if (crosshair_active)
                    {
                        crosshair_hide();
                        crosshair_active = false;
                    }
                    zoom_area_x = -1;
                    zoom_area_y = -1;
                    zoom_area_move(mouse_pos_x, mouse_pos_y);
                    zoom_area_active = true;
                    picker_hover_status = 0;
                    set_picker_hover_status("set area to zoom");
                }
                prev_ctrl_m = 1u;
            }
        }
        else
        {
            prev_ctrl_m = 0;
        }
        if (!canvas_input_locked() && !paste_preview_active &&
            key_pressed(HID_LEFT_CTRL) && shift_pressed() && alt_pressed() &&
            key_pressed(HID_F))
        {
            if (!prev_ctrl_shift_alt_t)
            {
                if (prepare_undo_step())
                {
                    canvas_modify_begin();
                    set_canvas_dirty(true);
                    draw_font_table(mouse_pos_x, mouse_pos_y);
                    canvas_modify_end();
                    snapshot_refresh_current();
                }
                prev_ctrl_shift_alt_t = 1u;
            }
        }
        else
        {
            prev_ctrl_shift_alt_t = 0;
        }
        mouse();
    }
}

/*
  SCRATCHPAD - do not touch from here
*/

/* unused but do not touch
static const uint8_t clear_pixel_mask_order[8][8] = {
    {0x80u, 0x40u, 0x20u, 0x10u, 0x08u, 0x04u, 0x02u, 0x01u},
    {0x01u, 0x02u, 0x04u, 0x08u, 0x10u, 0x20u, 0x40u, 0x80u},
    {0x40u, 0x10u, 0x04u, 0x01u, 0x80u, 0x20u, 0x08u, 0x02u},
    {0x02u, 0x08u, 0x20u, 0x80u, 0x01u, 0x04u, 0x10u, 0x40u},
    {0x20u, 0x04u, 0x80u, 0x01u, 0x40u, 0x02u, 0x10u, 0x08u},
    {0x08u, 0x10u, 0x02u, 0x40u, 0x01u, 0x80u, 0x04u, 0x20u},
    {0x10u, 0x80u, 0x08u, 0x01u, 0x20u, 0x02u, 0x40u, 0x04u},
    {0x04u, 0x40u, 0x02u, 0x20u, 0x01u, 0x10u, 0x08u, 0x80u}
};

static void clear_canvas_random_pixels(void)
{
    unsigned remaining;
    unsigned total;
    unsigned byte_index;
    uint8_t bit_index;
    uint8_t value;
    uint8_t mask;
    const uint8_t *order;

    total = CANVAS_STRIDE * GFX_CANVAS_HEIGHT;
    byte_index = (unsigned)prng_next();
    byte_index |= (unsigned)((unsigned)prng_next() << 8);
    if (byte_index >= total)
        byte_index -= total;

    RIA.step0 = 0;
    for (remaining = total; remaining != 0u; remaining--)
    {
        if ((remaining & 0x3Fu) == 0u && operation_cancel_requested())
            break;
        RIA.addr0 = CANVAS_DATA + byte_index;
        value = RIA.rw0;
        if (value != 0u)
        {
            order = clear_pixel_mask_order[prng_next() & 7u];
            for (bit_index = 0; bit_index < 8u; bit_index++)
            {
                mask = order[bit_index];
                if (!(value & mask))
                    continue;
                value &= (uint8_t)~mask;
                RIA.rw0 = value;
            }
        }

        byte_index += 73u;
        if (byte_index >= total)
            byte_index -= total;
    }
}
*/
