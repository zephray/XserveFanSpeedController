/* Copyright 2020 Wenting Zhang
 * Released under MIT license 
 */

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "lcd.h"
#include "font.h"
#include "ui.h"

#ifdef LARGE_UI
const unsigned char ui_bg[168] = { /* 0X10,0X01,0X00,0X32,0X00,0X18, */
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0XCE,0X60,
0X00,0X00,0X06,0X66,0XC0,0XA8,0X90,0X00,0X00,0X05,0X55,0X40,0XCE,0X90,0X00,0X00,
0X06,0X75,0X40,0XA8,0XA0,0X00,0X00,0X05,0X45,0X40,0XAE,0X50,0X00,0X00,0X05,0X44,
0X40,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0XEE,0XE0,0X00,0X00,0X06,0X66,0XC0,0X88,0X40,0X00,
0X00,0X05,0X55,0X40,0XEE,0X40,0X00,0X00,0X06,0X75,0X40,0X28,0X40,0X00,0X00,0X05,
0X45,0X40,0XEE,0X40,0X00,0X00,0X05,0X44,0X40,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X46,0XE0,
0X00,0X00,0X06,0X66,0XC0,0XA8,0X40,0X00,0X00,0X05,0X55,0X40,0XE8,0X40,0X00,0X00,
0X06,0X75,0X40,0XA8,0X40,0X00,0X00,0X05,0X45,0X40,0XA6,0X40,0X00,0X00,0X05,0X44,
0X40,0X00,0X00,0X00,0X00,0X00,0X00,0X00,};

#define BG_OFFSET_X (5)
#define BG_OFFSET_Y (5)
#define BG_COLOR (0xce00)
#define ON_COLOR (0x328b)
#define ON_SHADOW (0x4c6e)
#define OFF_COLOR (0x7e2f)
#define OFF_SHADOW (0x7e70)
#define UI_WIDTH (50)
#define UI_HEIGHT (23)

static void _lcd_set_pixel_large(size_t x, size_t y, bool on);
#else
#define BG_COLOR (0x0000)
#define FG_COLOR (0xffff)
#endif

static uint16_t switch_endian_16(uint16_t v) {
    return (((v >> 8) & 0xff) | (v << 8));
}

static void _lcd_set_pixel(size_t x, size_t y, uint16_t c) {
    //framebuffer[y * LCD_WIDTH + x] = switch_endian_16(c);
    framebuffer[y * LCD_WIDTH + x] = switch_endian_16(c);
}

void ui_disp_char(size_t x, size_t y, char c) {
    c -= 0x20;
    for (int yy = 0; yy < 7; yy++) {
        for (int xx = 0; xx < 5; xx++) {
            if ((font[c * 5 + xx] >> yy) & 0x01) {
#ifdef LARGE_UI
                _lcd_set_pixel_large(x + xx, y + yy, 1);
#else
                _lcd_set_pixel(x + xx, y + yy, FG_COLOR);
#endif
            }
            else {
#ifdef LARGE_UI
                _lcd_set_pixel_large(x + xx, y + yy, 0);
#else
                _lcd_set_pixel(x + xx, y + yy, BG_COLOR);
#endif
            }
        }
    }
}

void ui_disp_string(size_t x, size_t y, char *str) {
    while (*str) {
        ui_disp_char(x, y, *str++);
        x += 6;
    }
}

static char hex_to_char(int i) {
    if (i < 10)
        return '0' + i;
    else
        return 'A' + i - 10;
}

void ui_disp_hex(size_t x, size_t y, uint32_t num) {
    ui_disp_char(x, y, hex_to_char((num >> 12) & 0xf));
    ui_disp_char(x + 6, y, hex_to_char((num >> 8) & 0xf));
    ui_disp_char(x + 12, y, hex_to_char((num >> 4) & 0xf));
    ui_disp_char(x + 18, y, hex_to_char((num) & 0xf));
}

void ui_disp_num(size_t x, size_t y, uint32_t num) {
    if (num < 10000) {
        // Display 4 digit number
        ui_disp_char(x, y, num / 1000 + '0');
        num = num % 1000;
        ui_disp_char(x + 6, y, num / 100 + '0');
        num = num % 100;
        ui_disp_char(x + 12, y, num / 10 + '0');
        num = num % 10;
        ui_disp_char(x + 18, y, num + '0');
    }
    else {
        ui_disp_char(x, y, num / 10000 + '0');
        num = num % 10000;
        ui_disp_char(x + 6, y, num / 1000 + '0');
        ui_disp_char(x + 12, y, ' ');
        ui_disp_char(x + 18, y, 'K');
    }
}

#ifdef LARGE_UI
static void _lcd_set_pixel_large(size_t x, size_t y, bool on) {

    if (x >= UI_WIDTH) return;
    if (y >= UI_HEIGHT) return;

    uint16_t color = (on) ? ON_COLOR : OFF_COLOR;
    uint16_t shadow = (on) ? ON_SHADOW : OFF_SHADOW;
    _lcd_set_pixel(BG_OFFSET_X + x * 3,     BG_OFFSET_Y + y * 3, color);
    _lcd_set_pixel(BG_OFFSET_X + x * 3 + 1, BG_OFFSET_Y + y * 3, color);
    _lcd_set_pixel(BG_OFFSET_X + x * 3,     BG_OFFSET_Y + y * 3 + 1, color);
    _lcd_set_pixel(BG_OFFSET_X + x * 3 + 1, BG_OFFSET_Y + y * 3 + 1, color);

    _lcd_set_pixel(BG_OFFSET_X + x * 3 + 2, BG_OFFSET_Y + y * 3 + 0, shadow);
    _lcd_set_pixel(BG_OFFSET_X + x * 3 + 2, BG_OFFSET_Y + y * 3 + 1, shadow);
    _lcd_set_pixel(BG_OFFSET_X + x * 3 + 2, BG_OFFSET_Y + y * 3 + 2, shadow);
    _lcd_set_pixel(BG_OFFSET_X + x * 3 + 1, BG_OFFSET_Y + y * 3 + 2, shadow);
    _lcd_set_pixel(BG_OFFSET_X + x * 3,     BG_OFFSET_Y + y * 3 + 2, shadow);
    
}

void ui_clear (void) {
    for (int i = 0; i < UI_WIDTH; i++) {
        for (int j = 0; j < UI_HEIGHT; j++) {
            _lcd_set_pixel_large(i, j, 0);
        }
    }
}

void ui_disp_bg(uint8_t *img) {
    int i = 0;
    for (int y = 0; y < UI_HEIGHT; y++) {
        for (int x = 0; x < (UI_WIDTH + 7) / 8; x++) {
            uint8_t p = img[i++];
            for (int z = 0; z < 8; z++) {
                _lcd_set_pixel_large(x * 8 + z, y, (p << z) & 0x80);
            }
            
        }
    }
}

void ui_init(void) {
    for (int i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) {
        framebuffer[i] = switch_endian_16(BG_COLOR);
    }
    ui_disp_bg((uint8_t *)ui_bg);
}
#else
// Small UI

void ui_init(void) {
    for (int i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) {
        framebuffer[i] = switch_endian_16(BG_COLOR);
    }

    for (int i = 0; i < LCD_WIDTH; i++) {
        _lcd_set_pixel(i, 10, FG_COLOR);
    }
    ui_disp_string(3, 0, "REQ");
    ui_disp_string(31, 0, "SET");
    ui_disp_string(59, 0, "ACT");
}
#endif