/*
 * PaintHD loader
 * Copyright (c) 2026 WojciechGw
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "paintHDloader.h"

#define RELEASE

#define WITHMALLOC

#ifdef WITHMALLOC
    void *__fastcall__ argv_mem(size_t size) { return malloc(size); }
#else
    void *__fastcall__ argv_mem(size_t size) {
        static uint8_t buf[512];
        if (size > sizeof(buf)) return NULL;
        return buf;
    }
#endif

static int16_t mouse_pos_x;
static int16_t mouse_pos_y;
static uint8_t mouse_stack[8];
static char *startup_program_dir;
static char **startup_bmp_names;
static unsigned *startup_bmp_dates;
static unsigned *startup_bmp_times;
static unsigned startup_bmp_count;
static unsigned startup_bmp_capacity;
static unsigned startup_page;
static f_stat_t startup_dirent;
static uint8_t startup_bmp_hdr[34];

unsigned char mouse_irq_fn(void)
{
    static int16_t raw_x;
    static int16_t raw_y;
    static uint8_t prev_x;
    static uint8_t prev_y;
    uint8_t save_step0;
    uint16_t save_addr0;
    uint8_t rw;

    VIA.ifr = 0x40u;
    save_step0 = RIA.step0;
    save_addr0 = RIA.addr0;

    RIA.addr0 = MOUSE_INPUT_X;
    RIA.step0 = 0;
    rw = RIA.rw0;
    raw_x += (int8_t)(rw - prev_x);
    prev_x = rw;

    if (raw_x < 0)
        raw_x = 0;
    if (raw_x >= (int)GFX_CANVAS_WIDTH)
        raw_x = (int)GFX_CANVAS_WIDTH - 1;

    RIA.addr0 = MOUSE_INPUT_Y;
    rw = RIA.rw0;
    raw_y += (int8_t)(rw - prev_y);
    prev_y = rw;

    if (raw_y < 0)
        raw_y = 0;
    if (raw_y >= (int)GFX_CANVAS_HEIGHT)
        raw_y = (int)GFX_CANVAS_HEIGHT - 1;

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

    // 125Hz from the VIA
    unsigned timer_val = (ria_attr_get(RIA_ATTR_PHI2_KHZ) * 8u) - 2u;
    // 200Hz from the VIA
    // unsigned timer_val = (ria_attr_get(RIA_ATTR_PHI2_KHZ) * 5u) - 2u;
    // 400Hz from the VIA
    // unsigned timer_val = (ria_attr_get(RIA_ATTR_PHI2_KHZ) * 5u) / 2u - 2u;

    VIA.t1l_lo = timer_val & 0xFFu;
    VIA.t1l_hi = timer_val >> 8;
    VIA.t1_lo = timer_val & 0xFFu;
    VIA.t1_hi = timer_val >> 8;
    VIA.acr = 0x40u;
    VIA.ier = 0xC0u;
    xreg_ria_mouse(MOUSE_INPUT);
    set_irq(mouse_irq_fn, &mouse_stack, sizeof(mouse_stack));

}

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

/*
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
        tile_index = (unsigned)(state - 1u);
        if (tile_index < ((unsigned)GFX_CANVAS_WIDTH / 8u) * ((unsigned)GFX_CANVAS_HEIGHT / 8u))
        {
            tile_x = tile_index % (GFX_CANVAS_WIDTH / 8u);
            tile_y = tile_index / (GFX_CANVAS_WIDTH / 8u);
            base = CANVAS_DATA + tile_y * (unsigned)(CANVAS_STRIDE * 8u) + tile_x;

            for (row = 0u; row < 8u; row++)
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
*/

static void raw_set_pixel(int x, int y, uint8_t color)
{
    uint8_t mask;

    if (x < 0 || x >= (int)GFX_CANVAS_WIDTH || y < 0 || y >= (int)GFX_CANVAS_HEIGHT)
        return;

    RIA.step0 = 0;
    RIA.addr0 = CANVAS_DATA + (unsigned)y * CANVAS_STRIDE + ((unsigned)x >> 3);
    mask = (uint8_t)(0x80u >> (x & 7));
    if (color)
        RIA.rw0 = RIA.rw0 | mask;
    else
        RIA.rw0 = RIA.rw0 & (uint8_t)~mask;
}

static void startup_enter_program_dir(const char *argv0)
{
    unsigned len;
    unsigned i;
    unsigned last_sep;
    char *path;

    if (argv0 == 0 || argv0[0] == '\0')
        return;

    len = (unsigned)strlen(argv0);
    last_sep = len;
    for (i = 0u; i < len; i++)
    {
        if (argv0[i] == '/' || argv0[i] == '\\')
            last_sep = i;
    }
    if (last_sep == len || last_sep == 0u)
        return;

    path = malloc(last_sep + 1u);
    if (path == 0)
        return;
    memcpy(path, argv0, last_sep);
    path[last_sep] = '\0';
    startup_program_dir = path;
}

static const char *startup_basename(const char *path)
{
    const char *name;

    name = path;
    while (*path != '\0')
    {
        if (*path == '/' || *path == '\\')
            name = path + 1;
        path++;
    }
    return name;
}

static char *startup_make_path(const char *dir, const char *name)
{
    unsigned dir_len;
    unsigned name_len;
    char *path;

    if (dir == 0 || dir[0] == '\0')
    {
        path = malloc(strlen(name) + 1u);
        if (path == 0)
            return 0;
        strcpy(path, name);
        return path;
    }

    dir_len = (unsigned)strlen(dir);
    name_len = (unsigned)strlen(name);
    path = malloc(dir_len + name_len + 2u);
    if (path == 0)
        return 0;
    memcpy(path, dir, dir_len);
    if (dir_len != 0u && dir[dir_len - 1u] != '/' && dir[dir_len - 1u] != '\\')
        path[dir_len++] = '/';
    memcpy(path + dir_len, name, name_len);
    path[dir_len + name_len] = '\0';
    return path;
}

static uint8_t startup_bmp_name_match(const char *name)
{
    unsigned len;

    len = (unsigned)strlen(name);
    if (len < 4u)
        return 0u;
    if (name[len - 4u] != '.')
        return 0u;
    if (tolower((unsigned char)name[len - 3u]) != 'b')
        return 0u;
    if (tolower((unsigned char)name[len - 2u]) != 'm')
        return 0u;
    if (tolower((unsigned char)name[len - 1u]) != 'p')
        return 0u;
    return 1u;
}

static void startup_clear_bmps(void)
{
    unsigned i;

    for (i = 0u; i < startup_bmp_count; i++)
    {
        if (startup_bmp_names[i] != 0)
        {
            free(startup_bmp_names[i]);
        }
    }
    free(startup_bmp_names);
    free(startup_bmp_dates);
    free(startup_bmp_times);
    startup_bmp_names = 0;
    startup_bmp_dates = 0;
    startup_bmp_times = 0;
    startup_bmp_count = 0u;
    startup_bmp_capacity = 0u;
    startup_page = 0u;
}

static uint8_t startup_ensure_capacity(unsigned count)
{
    char **new_names;
    unsigned *new_dates;
    unsigned *new_times;
    unsigned new_capacity;

    if (count <= startup_bmp_capacity)
        return 1u;

    new_capacity = startup_bmp_capacity == 0u ? 16u : startup_bmp_capacity;
    while (new_capacity < count)
        new_capacity += 16u;

    new_names = realloc(startup_bmp_names, new_capacity * sizeof(*new_names));
    if (new_names == 0)
        return 0u;
    startup_bmp_names = new_names;

    new_dates = realloc(startup_bmp_dates, new_capacity * sizeof(*new_dates));
    if (new_dates == 0)
        return 0u;
    startup_bmp_dates = new_dates;

    new_times = realloc(startup_bmp_times, new_capacity * sizeof(*new_times));
    if (new_times == 0)
        return 0u;
    startup_bmp_times = new_times;

    startup_bmp_capacity = new_capacity;
    return 1u;
}

static void startup_insert_bmp(const char *name, unsigned fdate, unsigned ftime)
{
    unsigned pos;
    unsigned count;
    unsigned len;
    char *copy;

    pos = 0u;
    while (pos < startup_bmp_count)
    {
        if (fdate > startup_bmp_dates[pos] ||
            (fdate == startup_bmp_dates[pos] && ftime > startup_bmp_times[pos]))
            break;
        pos++;
    }
    len = (unsigned)strlen(name);
    copy = malloc(len + 1u);
    if (copy == 0)
        return;
    strcpy(copy, name);

    count = startup_bmp_count;
    if (!startup_ensure_capacity(count + 1u))
    {
        free(copy);
        return;
    }
    startup_bmp_count++;

    while (count > pos)
    {
        startup_bmp_names[count] = startup_bmp_names[count - 1u];
        startup_bmp_dates[count] = startup_bmp_dates[count - 1u];
        startup_bmp_times[count] = startup_bmp_times[count - 1u];
        count--;
    }

    startup_bmp_names[pos] = copy;
    startup_bmp_dates[pos] = fdate;
    startup_bmp_times[pos] = ftime;
}

static uint8_t startup_collect_bmps(void)
{
    int dir;
    char *path;

    startup_clear_bmps();
    dir = f_opendir(startup_program_dir != 0 ? startup_program_dir : ".");
    if (dir < 0)
        return 0u;

    while (f_readdir(&startup_dirent, dir) == 0)
    {
        if (startup_dirent.fname[0] == '\0')
            break;
        if (!startup_bmp_name_match(startup_dirent.fname))
            continue;
        path = startup_make_path(startup_program_dir, startup_dirent.fname);
        if (path == 0)
            continue;
        startup_insert_bmp(path, startup_dirent.fdate, startup_dirent.ftime);
        free(path);
    }

    f_closedir(dir);
    return startup_bmp_count;
}

static unsigned startup_page_base(void)
{
    return startup_page * STARTUP_TILE_COUNT;
}

/* Page 0 holds STARTUP_TILE_COUNT-1 files (tile 0 = CREATE NEW IMAGE).
   Subsequent pages hold STARTUP_TILE_COUNT files each. */
static unsigned startup_page_count(void)
{
    unsigned slots0 = STARTUP_TILE_COUNT - 1u;
    if (startup_bmp_count <= slots0)
        return 1u;
    return 1u + (startup_bmp_count - slots0 + STARTUP_TILE_COUNT - 1u) / STARTUP_TILE_COUNT;
}

static void startup_page_cache_path(unsigned page, char *out)
{
    memcpy(out, "./TMP/loader_p000.bin", 20u);
    out[12] = (char)('0' + ((page / 100u) % 10u));
    out[13] = (char)('0' + ((page / 10u) % 10u));
    out[14] = (char)('0' + (page % 10u));
}

static void startup_clear_page_cache(void)
{
    unsigned page;
    unsigned page_count;
    char path[STARTUP_CACHE_PATH_LEN];

    page_count = startup_page_count();
    for (page = 0u; page < page_count; page++)
    {
        startup_page_cache_path(page, path);
        unlink(path);
    }
}

static uint8_t startup_load_page_cache(unsigned page)
{
    char path[STARTUP_CACHE_PATH_LEN];
    uint16_t addr;
    uint16_t remaining;
    unsigned chunk;
    int fd;

    startup_page_cache_path(page, path);
    fd = open(path, O_RDONLY);
    if (fd < 0)
        return 0u;

    addr = CANVAS_DATA;
    remaining = (uint16_t)(GFX_CANVAS_HEIGHT * CANVAS_STRIDE);
    while (remaining > 0u)
    {
        chunk = (remaining > 0x7FFFu) ? 0x7FFFu : (unsigned)remaining;
        if (read_xram(addr, chunk, fd) != (int)chunk)
        {
            close(fd);
            return 0u;
        }
        addr += (uint16_t)chunk;
        remaining -= (uint16_t)chunk;
    }

    close(fd);
    return 1u;
}

static void startup_save_page_cache(unsigned page)
{
    char path[STARTUP_CACHE_PATH_LEN];
    uint16_t addr;
    uint16_t remaining;
    unsigned chunk;
    int fd;

    startup_page_cache_path(page, path);
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;

    addr = CANVAS_DATA;
    remaining = (uint16_t)(GFX_CANVAS_HEIGHT * CANVAS_STRIDE);
    while (remaining > 0u)
    {
        chunk = (remaining > 0x7FFFu) ? 0x7FFFu : (unsigned)remaining;
        if (write_xram(addr, chunk, fd) < 0)
            break;
        addr += (uint16_t)chunk;
        remaining -= (uint16_t)chunk;
    }

    close(fd);
}

static void draw_text_char(char ch, int px, int py, uint8_t fg_color, uint8_t bg_color)
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

static void draw_text(const char *text, int px, int py, int max_x, uint8_t fg_color, uint8_t bg_color)
{
    while (*text != '\0' && (px + 4) <= max_x)
    {
        draw_text_char(*text, px, py, fg_color, bg_color);
        px += 6;
        text++;
    }
}

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

static void draw_header_bar(void)
{
    static const char loader_name[] = " Select the image you want to work on ";
    char total_text[32];
    char page_text[32];
    char keys_text[16];
    unsigned page_count;
    unsigned row;
    unsigned col;
    int py;
    int total_x;
    int page_w;
    int keys_w;
    int page_x;
    int keys_x;

    page_count = startup_page_count();

    for (row = STATUSBAR_START_AT_ROW; row < GFX_CANVAS_HEIGHT; row++)
    {
        RIA.step0 = 1;
        RIA.addr0 = CANVAS_DATA + row * CANVAS_STRIDE;
        for (col = 0u; col < CANVAS_STRIDE; col++)
            RIA.rw0 = 0u;
    }

    py = (int)(STATUSBAR_START_AT_ROW);

    draw_text(loader_name, 0, py, (int)GFX_CANVAS_WIDTH - 1, BLACK, WHITE);

    sprintf(total_text, " total images : %u ", startup_bmp_count);
    total_x = ((int)GFX_CANVAS_WIDTH - text_width(total_text)) / 2;
    if (total_x < 0)
        total_x = 0;
    draw_text(total_text, total_x, py, (int)GFX_CANVAS_WIDTH - 1, WHITE, BLACK);

    page_text[0] = '\0';
    keys_text[0] = '\0';
    if (page_count > 1u)
    {
        if (startup_page > 0u)
            strcat(keys_text, "PgUp");
        if ((startup_page + 1u) < page_count)
        {
            if (keys_text[0] != '\0')
                strcat(keys_text, "/");
            strcat(keys_text, "PgDn");
        }
        sprintf(page_text, " Page %u/%u ", startup_page + 1u, page_count);
    }
    else
    {
        sprintf(page_text, " Page 1/1 ");
    }

    page_w = text_width(page_text);
    keys_w = 0;
    if (keys_text[0] != '\0')
        keys_w = 4 + text_width("use ") + text_width(keys_text);
    page_x = (int)GFX_CANVAS_WIDTH - 5 - page_w;
    if (page_x < 0)
        page_x = 0;
    if (keys_text[0] != '\0')
    {
        keys_x = page_x - 10 - text_width(keys_text) - 1 - text_width("use");
        draw_text("use", keys_x, py, page_x - 1, WHITE, BLACK);
        draw_text(keys_text, keys_x + text_width("use "), py, page_x - 1, WHITE, BLACK);
    }
    draw_text(page_text, page_x + 5, py, (int)GFX_CANVAS_WIDTH - 1, BLACK, WHITE);
}

static void draw_tiles(void)
{
    unsigned i;
    int tile_x;
    int tile_y;
    unsigned tile_bx;
    int x;
    int y;
    int x2;
    int y2;
    int label_y;
    int label_w;
    int fd;
    int ty;

    int bx;
    uint16_t pixel_offset;
    uint8_t top_down;
    uint8_t pattern;
    uint16_t bmp_width_px;
    uint16_t abs_height;
    uint16_t file_stride;
    int16_t  bmp_h_signed;
    long row_offset;
    const char *label;
    unsigned file_index;
    unsigned page_base;

    if (startup_load_page_cache(startup_page))
        return;

    draw_header_bar();
    page_base = startup_page_base();
    for (i = 0u; i < STARTUP_TILE_COUNT; i++)
    {
        tile_x = (int)(STARTUP_LEFT_MARGIN + (i % STARTUP_TILE_COLS) * (STARTUP_TILE_WIDTH + STARTUP_TILE_SPACING_X));
        tile_y = (int)(STARTUP_TOP_MARGIN + (i / STARTUP_TILE_COLS) * (STARTUP_TILE_HEIGHT + STARTUP_TILE_SPACING_Y));
        tile_bx = (unsigned)tile_x >> 3;
        x2 = tile_x + (int)STARTUP_TILE_WIDTH - 1;
        y2 = tile_y + (int)STARTUP_TILE_HEIGHT - 1;

        if (page_base == 0u && i == 0u)
        {
            /* CREATE NEW IMAGE tile — draw black */
            for (y = tile_y; y <= y2; y++)
            {
                RIA.step0 = 1;
                RIA.addr0 = CANVAS_DATA + (unsigned)y * CANVAS_STRIDE + tile_bx;
                for (bx = 0; bx < (int)(STARTUP_TILE_WIDTH / 8u); bx++)
                    RIA.rw0 = 0x00u;
            }
            label_y = tile_y - 2;
            for (y = label_y; y < label_y + (int)STARTUP_LABEL_HEIGHT; y++)
                for (x = tile_x - 2; x <= x2; x++)
                    raw_set_pixel(x, y, WHITE);
            draw_text("CREATE NEW IMAGE", tile_x - 2 + 2, label_y + 1, x2 - 1, BLACK, WHITE);
            continue;
        }

        file_index = page_base + i - 1u;

        if (file_index < startup_bmp_count)
        {
            fd = open(startup_bmp_names[file_index], O_RDONLY);
            if (fd >= 0)
            {
                if (read(fd, startup_bmp_hdr, sizeof(startup_bmp_hdr)) == (int)sizeof(startup_bmp_hdr) &&
                    startup_bmp_hdr[0] == 'B' && startup_bmp_hdr[1] == 'M')
                {
                    pixel_offset = (uint16_t)startup_bmp_hdr[10] | ((uint16_t)startup_bmp_hdr[11] << 8);
                    bmp_width_px  = (uint16_t)startup_bmp_hdr[18] | ((uint16_t)startup_bmp_hdr[19] << 8);
                    bmp_h_signed  = (int16_t)((uint16_t)startup_bmp_hdr[22] | ((uint16_t)startup_bmp_hdr[23] << 8));
                    top_down      = (bmp_h_signed < 0);
                    abs_height    = top_down ? (uint16_t)(-bmp_h_signed) : (uint16_t)bmp_h_signed;
                    file_stride   = ((bmp_width_px + 31u) / 32u) * 4u;
                    if (file_stride > CANVAS_STRIDE * 4u) file_stride = CANVAS_STRIDE * 4u;

                    for (y = tile_y; y <= y2; y++)
                    {
                        RIA.step0 = 1;
                        RIA.addr0 = CANVAS_DATA + (unsigned)y * CANVAS_STRIDE + tile_bx;
                        for (bx = 0; bx < (int)(STARTUP_TILE_WIDTH / 8u); bx++)
                            RIA.rw0 = 0x00u;
                    }

                    {
                        /* bytes per tile row (160px / 8 = 20) */
                        uint16_t tile_bytes = STARTUP_TILE_WIDTH / 8u;
                        /* bytes to read per file row: min(file_stride, tile_bytes) */
                        uint16_t read_bytes = (file_stride < tile_bytes) ? file_stride : tile_bytes;
                        /* bytes to skip in file after each read to reach next row */
                        uint16_t skip_bytes = file_stride - read_bytes;
                        /* rows to copy: min(abs_height, STARTUP_TILE_HEIGHT) */
                        uint16_t rows_to_show = (abs_height < (uint16_t)STARTUP_TILE_HEIGHT)
                                                ? abs_height
                                                : (uint16_t)STARTUP_TILE_HEIGHT;
                        uint16_t first_file_row;
                        int dest;

                        if (top_down)
                        {
                            if (lseek(fd, (long)pixel_offset, SEEK_SET) < 0)
                                goto tile_close;
                            for (ty = 0; ty < (int)rows_to_show; ty++)
                            {
                                read_xram(CANVAS_DATA + (unsigned)(tile_y + ty) * CANVAS_STRIDE + tile_bx,
                                          read_bytes, fd);
                                if (skip_bytes > 0)
                                    lseek(fd, (long)skip_bytes, SEEK_CUR);
                            }
                        }
                        else
                        {
                            /* bottom-up: row 0 in file = bottom of image.
                               To show top rows: seek to last rows_to_show file rows. */
                            first_file_row = (abs_height > (uint16_t)STARTUP_TILE_HEIGHT)
                                             ? abs_height - (uint16_t)STARTUP_TILE_HEIGHT
                                             : 0u;
                            row_offset = (long)pixel_offset + (long)first_file_row * (long)file_stride;
                            if (lseek(fd, row_offset, SEEK_SET) < 0)
                                goto tile_close;
                            for (ty = 0; ty < (int)rows_to_show; ty++)
                            {
                                /* file reads bottom→top of image, tile dest is top→bottom */
                                dest = (int)rows_to_show - 1 - ty;
                                read_xram(CANVAS_DATA + (unsigned)(tile_y + dest) * CANVAS_STRIDE + tile_bx,
                                          read_bytes, fd);
                                if (skip_bytes > 0)
                                    lseek(fd, (long)skip_bytes, SEEK_CUR);
                            }
                        }
                    }
                    tile_close:;
                }
                close(fd);
            }
        }
        else
        {
            for (y = tile_y; y <= y2; y++)
            {
                pattern = ((y - tile_y) & 1) ? 0x55u : 0xAAu;
                RIA.step0 = 1;
                RIA.addr0 = CANVAS_DATA + (unsigned)y * CANVAS_STRIDE + tile_bx;
                for (bx = 0; bx < (int)(STARTUP_TILE_WIDTH / 8u); bx++)
                    RIA.rw0 = pattern;
            }
        }

        if (file_index < startup_bmp_count)
        {
            label = startup_basename(startup_bmp_names[file_index]);
            label_w = 4 + (int)(strlen(label) * 6u);
            if (label_w > (int)STARTUP_TILE_WIDTH)
                label_w = (int)STARTUP_TILE_WIDTH;
            label_y = tile_y - 2;
            for (y = label_y; y < label_y + (int)STARTUP_LABEL_HEIGHT; y++)
            {
                for (x = tile_x - 2; x < tile_x - 2 + label_w; x++)
                    raw_set_pixel(x, y, WHITE);
            }
            draw_text(label, tile_x - 2 + 2, label_y + 1, tile_x - 2 + label_w - 3, BLACK, WHITE);
        }
    }
    startup_save_page_cache(startup_page);
}

static int tile_hit(int x, int y)
{
    unsigned col;
    unsigned row;
    unsigned index;

    if (x < 0 || x >= (int)GFX_CANVAS_WIDTH || y < 0 || y >= (int)GFX_CANVAS_HEIGHT)
        return -1;
    if (x < (int)STARTUP_LEFT_MARGIN || y < (int)STARTUP_TOP_MARGIN)
        return -1;

    col = ((unsigned)x - STARTUP_LEFT_MARGIN) / (STARTUP_TILE_WIDTH + STARTUP_TILE_SPACING_X);
    row = ((unsigned)y - STARTUP_TOP_MARGIN) / (STARTUP_TILE_HEIGHT + STARTUP_TILE_SPACING_Y);
    if (col >= STARTUP_TILE_COLS || row >= STARTUP_TILE_ROWS)
        return -1;
    /* reject clicks in the spacing gap */
    if (((unsigned)x - STARTUP_LEFT_MARGIN) % (STARTUP_TILE_WIDTH + STARTUP_TILE_SPACING_X) >= STARTUP_TILE_WIDTH)
        return -1;
    if (((unsigned)y - STARTUP_TOP_MARGIN) % (STARTUP_TILE_HEIGHT + STARTUP_TILE_SPACING_Y) >= STARTUP_TILE_HEIGHT)
        return -1;
    index = startup_page_base() + row * STARTUP_TILE_COLS + col;
    return (int)index;
}

static int wait_for_selection(void)
{
    uint8_t buttons;
    uint8_t prev_buttons;
    uint8_t pageup_now;
    uint8_t pagedown_now;
    uint8_t prev_pageup;
    uint8_t prev_pagedown;
    int index;
    unsigned page_count;

    prev_buttons = 0u;
    prev_pageup = 0u;
    prev_pagedown = 0u;
    page_count = startup_page_count();
    while (1)
    {
        pageup_now = key_pressed(HID_PAGEUP) ? 1u : 0u;
        pagedown_now = key_pressed(HID_PAGEDOWN) ? 1u : 0u;
        if (pageup_now && !prev_pageup && startup_page > 0u)
        {
            startup_page--;
            draw_tiles();
        }
        if (pagedown_now && !prev_pagedown && (startup_page + 1u) < page_count)
        {
            startup_page++;
            draw_tiles();
        }
        prev_pageup = pageup_now;
        prev_pagedown = pagedown_now;

        RIA.addr0 = MOUSE_INPUT_BUTTONS;
        RIA.step0 = 0;
        buttons = RIA.rw0;
        if ((buttons & 1u) != 0u && (prev_buttons & 1u) == 0u)
        {
            index = tile_hit(mouse_pos_x, mouse_pos_y);
            if (index == 0 || (index > 0 && (unsigned)index - 1u < startup_bmp_count))
                return index;
        }
        prev_buttons = buttons;
    }
}

int main(int argc, char *argv[])
{
    int selected;

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

    xram0_struct_set(POINTER_STRUCT, vga_mode3_config_t, x_wrap, false);
    xram0_struct_set(POINTER_STRUCT, vga_mode3_config_t, y_wrap, false);
    xram0_struct_set(POINTER_STRUCT, vga_mode3_config_t, width_px, POINTER_WIDTH);
    xram0_struct_set(POINTER_STRUCT, vga_mode3_config_t, height_px, POINTER_HEIGHT);
    xram0_struct_set(POINTER_STRUCT, vga_mode3_config_t, xram_data_ptr, XRAM_POINTERS_arrow);
    xram0_struct_set(POINTER_STRUCT, vga_mode3_config_t, xram_palette_ptr, 0xFFFF);

    if (argc > 0)
        startup_enter_program_dir(argv[0]);

    {
        char *tmp_path = startup_make_path(startup_program_dir, "TMP");
        if (tmp_path != 0)
        {
            f_mkdir(tmp_path);
            free(tmp_path);
        }
    }

    xreg_vga_mode(GFX_MODE_BITMAP, GFX_BITMAP_bpp1, CANVAS_STRUCT, GFX_PLANE_0);
    xreg_ria_keyboard(KEYBOARD_INPUT);
    /*
    if(argc == 1)
    {
        PAUSE(PAUSE_TICKS_START);
        clear_canvas_random_blocks8();
    }
    */
    xreg_vga_mode(GFX_MODE_BITMAP, GFX_BITMAP_bpp8, POINTER_STRUCT, GFX_PLANE_2);
    mouse_pos_x = (int16_t)(GFX_CANVAS_WIDTH / 2u);
    mouse_pos_y = (int16_t)(GFX_CANVAS_HEIGHT / 2u);
    xram0_struct_set(POINTER_STRUCT, vga_mode3_config_t, x_pos_px, mouse_pos_x);
    xram0_struct_set(POINTER_STRUCT, vga_mode3_config_t, y_pos_px, mouse_pos_y);
    draw_pointer(POINTER_hourglass);
    startup_collect_bmps();

    {
        char new_arg[] = "paintHD_new.bmp";
        unsigned file_index;

        startup_clear_page_cache();
        draw_tiles();
        draw_pointer(POINTER_arrow);
        mouse_init();
        selected = wait_for_selection();
        VIA.ier = 0x40; /* disable T1 interrupt (mouse) */
        if (selected == 0)
        {
            ria_execl("paintHDeditor.rp6502", new_arg, NULL);
            return 0;
        }
        file_index = (unsigned)selected - 1u;
        if (file_index < startup_bmp_count)
        {
            #ifdef DEBUG
            printf("%s\n", startup_basename(startup_bmp_names[file_index]));
            #endif
            ria_execl("paintHDeditor.rp6502", startup_basename(startup_bmp_names[file_index]), NULL);
            return 0;
        }
    }
}
