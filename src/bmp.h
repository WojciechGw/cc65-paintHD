/*
 * BMP 1bpp library
 * Copyright (c) 2026 WojciechGw
*/

#ifndef BMP_H
#define BMP_H

#include <rp6502.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int LoadBMP(const char *path, uint16_t xram_address, uint16_t height, uint16_t width);
int SaveBMP(const char *path, uint16_t xram_address, uint16_t height, uint16_t width);
int DumpBIN(const char *path, uint16_t xram_address, uint16_t height, uint16_t width) ;
int FontLoadBMP(char *path, uint16_t xram_address);

#endif /* PAINT_HD_H */