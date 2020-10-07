/* Copyright 2020 Wenting Zhang
 * Released under MIT license 
 * 
 * Driver for ST7735 LCD on Longan Nano board 
 */
#pragma once

//#define LCD_HORIZONTAL
#define LCD_VERTICAL

#ifdef LCD_HORIZONTAL
#define LCD_WIDTH (160)
#define LCD_HEIGHT (80)
#define LCD_OFFSET_X (1)
#define LCD_OFFSET_Y (26)
#else
#define LCD_WIDTH (80)
#define LCD_HEIGHT (160)
#define LCD_OFFSET_X (26)
#define LCD_OFFSET_Y (1)
#endif

extern uint16_t framebuffer[LCD_WIDTH * LCD_HEIGHT];

void lcd_init(void);
void lcd_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void lcd_clear(uint16_t color);
void lcd_update(void);