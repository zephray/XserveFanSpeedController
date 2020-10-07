/* Copyright 2020 Wenting Zhang
 * Released under MIT license 
 */
#include <stdlib.h>
#include "gd32vf103_gpio.h"
#include "gd32vf103_rcu.h"
#include "softi2c.h"
#include "fanslave.h"

// Why GD decides to define it as UPPER CASE opposed to lower case in stdc?
#define true TRUE
#define false FALSE

typedef enum {
    FS_IDLE,
    FS_SINGLE,
    FS_BLOCK_COUNT,
    FS_BLOCK_RW,
    FS_INVALID // Wait for STOP condition to reset
} FANSLAVE_STATE;

typedef struct {
    // IO related
    FANSLAVE_STATE state;
    uint8_t addr;
    uint32_t read_count;
    uint32_t cur_count;
    // Device related
    uint32_t id_base;
    uint8_t temp; // for 16 bit values
} FANSLAVE_CONTEXT;

#define SI2C0_GPIO    (GPIOB)
#define SI2C0_SCL_PIN (GPIO_PIN_12)
#define SI2C0_SDA_PIN (GPIO_PIN_13)
#define SI2C1_GPIO    (GPIOB)
#define SI2C1_SCL_PIN (GPIO_PIN_14)
#define SI2C1_SDA_PIN (GPIO_PIN_15)

static SI2C_CONTEXT si2c0;
static SI2C_CONTEXT si2c1;

static FANSLAVE_CONTEXT fs_context[7];

volatile uint32_t start_req;
volatile uint32_t rpm_update_req;

bool fs_enabled[14];
uint32_t fs_requested_tach[14];
uint32_t fs_actual_tach[14];

// Callback from SI2C driver
static void fanslave_si2c_write(uint32_t bus_id, uint8_t addr, uint8_t byte);
static void fanslave_si2c_read(uint32_t bus_id, uint8_t addr, uint8_t *byte,
        bool *last);
static void fanslave_si2c_stop(uint32_t bus_id, uint8_t addr);
// Called from SI2C CB, single device handler
static void fanslave_write_byte(FANSLAVE_CONTEXT *context, uint8_t byte);
static void fanslave_read_byte(FANSLAVE_CONTEXT *context, uint8_t *byte, bool *last);
static void fanslave_stop(FANSLAVE_CONTEXT *context);
// Actual device register RW handler
static void fanslave_write_reg(FANSLAVE_CONTEXT *context, uint8_t reg, uint8_t val);
static uint8_t fanslave_read_reg(FANSLAVE_CONTEXT *context, uint8_t reg);

void fanslave_init(void) {
    start_req = 0;
    rpm_update_req = 0;

    for (int i = 0; i < 7; i++) {
        fs_context[i].state = FS_IDLE;
        fs_context[i].read_count = 0;
        fs_context[i].id_base = i * 2;
    }

    si2c0.gpio = SI2C0_GPIO;
    si2c0.scl_pin = SI2C0_SCL_PIN;
    si2c0.sda_pin = SI2C0_SDA_PIN;
    si2c0.bus_id = 0;
    si2c0.read_cb = fanslave_si2c_read;
    si2c0.write_cb = fanslave_si2c_write;
    si2c0.stop_cb = fanslave_si2c_stop;

    si2c1.gpio = SI2C0_GPIO;
    si2c1.scl_pin = SI2C1_SCL_PIN;
    si2c1.sda_pin = SI2C1_SDA_PIN;
    si2c1.bus_id = 1;
    si2c1.read_cb = fanslave_si2c_read;
    si2c1.write_cb = fanslave_si2c_write;
    si2c1.stop_cb = fanslave_si2c_stop;

    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_AF);

    eclic_global_interrupt_enable();
    eclic_priority_group_set(ECLIC_PRIGROUP_LEVEL3_PRIO1);
    eclic_irq_enable(EXTI10_15_IRQn, 1, 1);

    gpio_exti_source_select(GPIO_PORT_SOURCE_GPIOB, GPIO_PIN_SOURCE_12);
    gpio_exti_source_select(GPIO_PORT_SOURCE_GPIOB, GPIO_PIN_SOURCE_13);
    gpio_exti_source_select(GPIO_PORT_SOURCE_GPIOB, GPIO_PIN_SOURCE_14);
    gpio_exti_source_select(GPIO_PORT_SOURCE_GPIOB, GPIO_PIN_SOURCE_15);

    si2c_init(&si2c0);
    si2c_init(&si2c1);
}

void EXTI10_15_IRQHandler(void) {
    if (exti_interrupt_flag_get(SI2C0_SCL_PIN)) {
        exti_interrupt_flag_clear(SI2C0_SCL_PIN);
        si2c_process(&si2c0, PIN_SCL);
    }

    if (exti_interrupt_flag_get(SI2C0_SDA_PIN)) {
        exti_interrupt_flag_clear(SI2C0_SDA_PIN);
        si2c_process(&si2c0, PIN_SDA);
    }

    if (exti_interrupt_flag_get(SI2C1_SCL_PIN)) {
        exti_interrupt_flag_clear(SI2C1_SCL_PIN);
        si2c_process(&si2c1, PIN_SCL);
    }

    if (exti_interrupt_flag_get(SI2C1_SDA_PIN)) {
        exti_interrupt_flag_clear(SI2C1_SDA_PIN);
        si2c_process(&si2c1, PIN_SDA);
    }
}

static FANSLAVE_CONTEXT *fanslave_id_to_context(uint32_t bus_id, uint8_t addr) {
    addr >>= 1;
    addr -= 0x50;
    uint32_t idx = bus_id * 4 + 3 - addr;
    // 0x53 -> 0
    // 0x52 -> 1
    // 0x51 -> 2
    // 0x50 -> 3
    if (idx > 7)
        return NULL;
    return &(fs_context[idx]);
}

static void fanslave_si2c_write(uint32_t bus_id, uint8_t addr, uint8_t byte) {
    if ((addr & 0xfe) == 0x82) {
        return; // ignore write
    }
    FANSLAVE_CONTEXT *context = fanslave_id_to_context(bus_id, addr);
    if (context != NULL)
        fanslave_write_byte(context, byte);
}

static void fanslave_si2c_read(uint32_t bus_id, uint8_t addr, uint8_t *byte,
        bool *last) {
    if ((addr & 0xfe) == 0x82) {
        // PCA9536, don't know the purpose, just return 0xFD
        *byte = 0xfd;
        *last = true;
    }
    FANSLAVE_CONTEXT *context = fanslave_id_to_context(bus_id, addr);
    if (context != NULL)
        fanslave_read_byte(context, byte, last);
}

static void fanslave_si2c_stop(uint32_t bus_id, uint8_t addr) {
    FANSLAVE_CONTEXT *context = fanslave_id_to_context(bus_id, addr);
    if (context != NULL)
        fanslave_stop(context);
}

static void fanslave_write_byte(FANSLAVE_CONTEXT *context, uint8_t byte) {
    switch (context->state) {
    case FS_IDLE:
        context->addr = byte & 0x7f;
        if (byte & 0x80) {
            // Block operation
            context->state = FS_BLOCK_COUNT;
        }
        else {
            context->state = FS_SINGLE;
        }
        break;
    case FS_SINGLE:
        fanslave_write_reg(context, context->addr, byte);
        context->state = FS_INVALID;
        break;
    case FS_BLOCK_COUNT:
        context->cur_count = byte;
        context->state = FS_BLOCK_RW;
        break;
    case FS_BLOCK_RW:
        fanslave_write_reg(context, context->addr, byte);
        context->addr ++;
        context->cur_count --;
        if (context->cur_count == 0)
            context->state = FS_INVALID;
        break;
    case FS_INVALID:
        // Wait for STOP condition, should not reach here
        break;
    }
}

static void fanslave_read_byte(FANSLAVE_CONTEXT *context, uint8_t *byte, bool *last) {
    switch (context->state) {
    case FS_IDLE:
        // Read at IDLE state??
        *byte = 0x00;
        *last = true;
        break;
    case FS_SINGLE:
        *byte = fanslave_read_reg(context, context->addr);
        *last = true;
        context->state = FS_INVALID;
        break;
    case FS_BLOCK_COUNT:
        *byte = context->read_count;
        context->cur_count = context->read_count;
        if (context->cur_count == 0) {
            *last = true;    
            context->state = FS_INVALID;
        }
        else {
            *last = false;
            context->state = FS_BLOCK_RW;
        }
        break;
    case FS_BLOCK_RW:
        *byte = fanslave_read_reg(context, context->addr);
        context->addr++;
        context->cur_count --;
        if (context->cur_count == 0) {
            *last = true;
            context->state = FS_INVALID;
        }
        else {
            *last = false;
        }
        break;
    case FS_INVALID:
        // Should not reach here
        break;
    }
}

static void fanslave_stop(FANSLAVE_CONTEXT *context) {
    context->state = FS_IDLE;
}

static void fanslave_write_reg(FANSLAVE_CONTEXT *context, uint8_t reg, uint8_t val) {
    if (reg == 0x00) {
        context->read_count = val;
    }
    else if (reg == 0x07) {
        fs_enabled[context->id_base] = !!(val & 0x40);
        fs_enabled[context->id_base + 1] = !!(val & 0x80);
    }
    else if (reg == 0x2a) {
        context->temp = val;
    }
    else if (reg == 0x2b) {
        fs_requested_tach[context->id_base] = ((uint32_t)val << 8) | 
                ((uint32_t)context->temp & 0xfful);
    }
    else if (reg == 0x2c) {
        context->temp = val;
    }
    else if (reg == 0x2d) {
        fs_requested_tach[context->id_base + 1] = ((uint32_t)val << 8) | 
                ((uint32_t)context->temp & 0xfful);

        if (context->id_base == 12) {
            rpm_update_req = 1;
            GPIO_BOP(GPIOA) = 0x01;
        }
    }
    else if (reg == 0x3c) {
        if (context->id_base == 12) {
            start_req = 1;
            GPIO_BOP(GPIOA) = 0x02;
        }
    }
}

static uint8_t fanslave_read_reg(FANSLAVE_CONTEXT *context, uint8_t reg) {
    if (reg == 0x4a) {
        return fs_actual_tach[context->id_base] & 0xff;
    }
    else if (reg == 0x4b) {
        return (fs_actual_tach[context->id_base] >> 8) & 0xff;
    }
    else if (reg == 0x4c) {
        return fs_actual_tach[context->id_base + 1] & 0xff;
    }
    else if (reg == 0x4d) {
        return (fs_actual_tach[context->id_base + 1] >> 8) & 0xff;
    }
    return 0x00;
}