/* Copyright 2020 Wenting Zhang
 * Released under MIT license 
 */
#pragma once

#include <stdint.h>

typedef void (*SI2C_WRITE_CB)(uint32_t bus_id, uint8_t addr, uint8_t byte);
typedef void (*SI2C_READ_CB)(uint32_t bus_id, uint8_t addr, uint8_t *byte, 
        bool *last);
typedef void (*SI2C_STOP_CB)(uint32_t bus_id, uint8_t addr);

typedef enum {
    ST_IDLE = 0,
    ST_ADDR = 1,
    ST_ADDR_ACK = 2,
    ST_READ_PREPARE = 3,
    ST_READ = 4,
    ST_READ_ACK = 5,
    ST_WRITE_PREPARE = 6,
    ST_WRITE = 7,
    ST_WRITE_ACK = 8,
    ST_WAIT_STOP = 9
} SI2C_STATE;

typedef enum {
    PIN_SCL,
    PIN_SDA
} SI2C_PIN;

typedef struct {
    uint32_t bus_id;
    uint32_t gpio;
    uint32_t sda_pin;
    uint32_t scl_pin;
    SI2C_READ_CB read_cb;
    SI2C_WRITE_CB write_cb;
    SI2C_STOP_CB stop_cb;
    SI2C_STATE state;
    uint32_t count;
    bool read_last;
    uint8_t addr;
    uint8_t data;
} SI2C_CONTEXT;

void si2c_init(SI2C_CONTEXT *context);
void si2c_process(SI2C_CONTEXT *context, SI2C_PIN pin);
