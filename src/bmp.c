/*
 * BMP 1bpp library
 * Copyright (c) 2026 WojciechGw
*/

#include "bmp.h"

#define KEYBOARD_INPUT 0xFF80
#define HID_ESCAPE 0x29
#define key_pressed(code) \
    (RIA.addr0 = KEYBOARD_INPUT + ((code) >> 3), RIA.step0 = 0, \
     RIA.rw0 & (1 << ((code) & 7)))

// LoadBMP - load BMP file 1-bit 640x480 or smaller to XRAM given address.
// handle both direction: bottom-up (standard BMP, height > 0)
// and top-down (height < 0, ie. files saved by SaveBMP).
// usage : LoadBMP("MSC0:/qrcode.bmp", GFX_DATA);
//

int LoadBMP(const char *path, uint16_t xram_address, uint16_t height, uint16_t width) {
    static uint8_t hdr[34];
    uint16_t pixel_offset;
    uint8_t  top_down;
    uint16_t bmp_width_px;
    int16_t  bmp_height_signed;
    uint16_t abs_height;
    uint16_t file_stride;
    uint16_t file_row;
    uint16_t xram_row;
    int fd;

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        return -1;
    }

    if (read(fd, hdr, 34) != 34 || hdr[0] != 'B' || hdr[1] != 'M') {
        close(fd);
        return -1;
    }

    pixel_offset      = (uint16_t)hdr[10] | ((uint16_t)hdr[11] << 8);
    bmp_width_px      = (uint16_t)hdr[18] | ((uint16_t)hdr[19] << 8);
    bmp_height_signed = (int16_t)((uint16_t)hdr[22] | ((uint16_t)hdr[23] << 8));
    top_down          = (bmp_height_signed < 0);
    abs_height        = top_down ? (uint16_t)(-bmp_height_signed) : (uint16_t)bmp_height_signed;

    /* BMP row stride: round pixel width up to 32-bit boundary */
    file_stride = ((bmp_width_px + 31u) / 32u) * 4u;

    /* clip to canvas limits */
    if (abs_height > height)    abs_height  = height;
    if (file_stride > width)    file_stride = width;

    lseek(fd, pixel_offset, SEEK_SET);

    for (file_row = 0; file_row < abs_height; file_row++) {
        if ((file_row & 0x0Fu) == 0u && key_pressed(HID_ESCAPE)) {
            close(fd);
            return -2;
        }
        xram_row = top_down ? file_row : (uint16_t)(abs_height - 1u - file_row);
        read_xram(xram_address + xram_row * width, file_stride, fd);
    }

    close(fd);
    return 0;
}

// SaveBMP - save framebuffer (640x480xbpp1) to BMP file.
// palette: index 0 = black, index 1 = white.
// usage: SaveBMP("MSC0:/drawing.bmp");
//
int SaveBMP(const char *path, uint16_t xram_address, uint16_t height, uint16_t width) {
    /* BMP header: File Header (14) + BITMAPINFOHEADER (40) + color table (8) = 62 bytes */
    static const uint8_t bmp_hdr[62] = {
        /* File Header */
        'B', 'M',
        0x5E, 0x96, 0x00, 0x00,  /* file size = 38462 = 0x965E */
        0x00, 0x00, 0x00, 0x00,  /* reserved */
        0x3E, 0x00, 0x00, 0x00,  /* pixel data offset = 62 */
        /* BITMAPINFOHEADER */
        0x28, 0x00, 0x00, 0x00,  /* header size = 40 */
        0x80, 0x02, 0x00, 0x00,  /* width  = 640 */
        0x20, 0xFE, 0xFF, 0xFF,  /* height = -480 (top-down) */
        0x01, 0x00,              /* color planes = 1 */
        0x01, 0x00,              /* bits per pixel = 1 */
        0x00, 0x00, 0x00, 0x00,  /* compression = BI_RGB */
        0x00, 0x00, 0x00, 0x00,  /* image size = 0 */
        0x13, 0x0B, 0x00, 0x00,  /* X ppm = 2835 (~72 dpi) */
        0x13, 0x0B, 0x00, 0x00,  /* Y ppm = 2835 */
        0x02, 0x00, 0x00, 0x00,  /* colors in table = 2 */
        0x00, 0x00, 0x00, 0x00,  /* important colors = 0 */
        /* Color table */
        0x00, 0x00, 0x00, 0x00,  /* index 0 = black */
        0xFF, 0xFF, 0xFF, 0x00   /* index 1 = white */
    };
    uint16_t addr;
    uint16_t datasize = (height * width);
    uint16_t remaining = datasize;
    unsigned chunk;
    int fd;

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) {
        return -1;
    }

    if (write(fd, bmp_hdr, sizeof(bmp_hdr)) != (int)sizeof(bmp_hdr)) {
        close(fd);
        return -1;
    }

    /* write pixel data in chunks <= 0x7FFF */
    addr      = xram_address;
    while (remaining > 0) {
        if (key_pressed(HID_ESCAPE)) {
            close(fd);
            return -2;
        }
        chunk = (remaining > 0x7FFF) ? 0x7FFF : (unsigned)remaining;
        if (write_xram(addr, chunk, fd) < 0) {
            close(fd);
            return -1;
        }
        addr      += (uint16_t)chunk;
        remaining -= (uint16_t)chunk;
    }

    close(fd);
    return 0;
}

int DumpBIN(const char *path, uint16_t xram_address, uint16_t height, uint16_t width) {

    int fd;
    uint16_t datasize = (height * width);
    uint16_t remaining = datasize;

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) {
        return -1;
    }
    while (remaining > 0) {
        unsigned chunk = (remaining > 0x7FFF) ? 0x7FFF : remaining;
        if (write_xram(xram_address, chunk, fd) < 0) break;
        xram_address      += (uint16_t)chunk;
        remaining -= (uint16_t)chunk;
    }
    close(fd);
    return 0;
}

// data size = 1024 bytes
int FontLoadBMP(char *path, uint16_t xram_address){

    int fd = 0;
    uint16_t i = 0;

    fd = open(path, O_RDONLY);
    if(fd >= 0 ){
        lseek(fd,0x003E,SEEK_SET);
        for(i = 16; i > 0; i--){
            read_xram(xram_address + 0x700 + ((i - 1) * 0x10), 0x10, fd);
            read_xram(xram_address + 0x600 + ((i - 1) * 0x10), 0x10, fd);
            read_xram(xram_address + 0x500 + ((i - 1) * 0x10), 0x10, fd);
            read_xram(xram_address + 0x400 + ((i - 1) * 0x10), 0x10, fd);
            read_xram(xram_address + 0x300 + ((i - 1) * 0x10), 0x10, fd);
            read_xram(xram_address + 0x200 + ((i - 1) * 0x10), 0x10, fd);
            read_xram(xram_address + 0x100 + ((i - 1) * 0x10), 0x10, fd);
            read_xram(xram_address + 0x000 + ((i - 1) * 0x10), 0x10, fd);
        }
        close(fd);
        return 0;
    } else {
        return -1;
    }

}
