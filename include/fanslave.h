/* Copyright 2020 Wenting Zhang
 * Released under MIT license 
 */
#pragma once

extern bool fs_enabled[14];
extern uint32_t fs_requested_tach[14];
extern uint32_t fs_actual_tach[14];

extern volatile uint32_t start_req;
extern volatile uint32_t rpm_update_req;

void fanslave_init(void);