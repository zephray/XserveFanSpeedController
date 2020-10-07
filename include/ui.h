/* Copyright 2020 Wenting Zhang
 * Released under MIT license
 */
#pragma once

//#define LARGE_UI
#define SMALL_UI

void ui_init(void);
void ui_disp_num(size_t x, size_t y, uint32_t num);
void ui_disp_hex(size_t x, size_t y, uint32_t num);