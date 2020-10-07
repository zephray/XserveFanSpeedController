/* Copyright 2020 Wenting Zhang
 * Released under MIT license 
 */
#pragma once

extern uint32_t fm_requested_tach[14];
extern uint32_t fm_actual_tach[14];

void fanmaster_init(void);
void fanmaster_start(void);
void fanmaster_set_tach(void);
void fanmaster_get_tach(void);