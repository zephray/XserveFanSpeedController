/* Copyright 2020 Wenting Zhang
 * Released under MIT license 
 */
#include <stdint.h>
#include "gd32vf103_gpio.h"
#include "gd32vf103_rcu.h"
#include "softi2c.h"

void si2c_init(SI2C_CONTEXT *context) {
    context->state = ST_IDLE;

    gpio_init(context->gpio, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ,
            context->scl_pin | context->sda_pin);

    // Debug
    gpio_init(GPIOA, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, 
            GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3);

    // Configure SDA line to trigger on falling edge, enabled
    EXTI_INTEN |= context->sda_pin;
    EXTI_RTEN &= ~ context->sda_pin;
    EXTI_FTEN |= context->sda_pin;

    // Configure SCL line to trigger on falling edge, disabled
    EXTI_INTEN &= ~ context->scl_pin;
    EXTI_RTEN &= ~ context->scl_pin;
    EXTI_FTEN |= context->scl_pin;

    // Clear interrupt pending flags
    EXTI_PD = context->sda_pin;
    EXTI_PD = context->scl_pin;
}

void si2c_process(SI2C_CONTEXT *context, SI2C_PIN pin) {
    static int dbg_toggle = 0;

    if (dbg_toggle) {
        dbg_toggle = 0;
        GPIO_BC(GPIOA) = 0x08;
    }
    else {
        dbg_toggle = 1;
        GPIO_BOP(GPIOA) = 0x08;
    }

    if (pin == PIN_SDA)
        GPIO_BC(GPIOA) = 0x04;
    else
        GPIO_BOP(GPIOA) = 0x04;

    // Stop condition, always reset the FSM
    if (pin == PIN_SDA) {
        if ((gpio_input_bit_get(context->gpio, context->sda_pin) == 1) &&
                (gpio_input_bit_get(context->gpio, context->scl_pin) == 1)) {
            // SDA goes high when SCL is high 
            context->state = ST_IDLE;
            // Trigger on falling edge for start condition
            EXTI_RTEN &= ~ context->sda_pin;
            EXTI_FTEN |= context->sda_pin;
            // Tell the handler function
            context->stop_cb(context->bus_id, context->addr);
            // Debug
            GPIO_BC(GPIOA) = 0x01;
            GPIO_BC(GPIOA) = 0x02;
            //GPIO_BC(GPIOA) = 0x04;
            //GPIO_BC(GPIOA) = 0x08;
        }
        else if (((context->state == ST_IDLE) || (context->state == ST_WRITE)) &&
                (gpio_input_bit_get(context->gpio, context->sda_pin) == 0) &&
                (gpio_input_bit_get(context->gpio, context->scl_pin) == 1)) {
            // SDA goes low when SCL is high
            // Start condition met, go to address phase.
            EXTI_FTEN &= ~ context->scl_pin;
            EXTI_RTEN |= context->scl_pin;
            EXTI_PD = context->scl_pin;
            EXTI_INTEN |= context->scl_pin;

            // Configure the SDA interrupt to monitor STOP condition or repeated
            // START condition
            EXTI_FTEN |= context->sda_pin;
            EXTI_RTEN |= context->sda_pin;
            //EXTI_INTEN &= ~context->sda_pin;

            context->count = 0;
            context->state = ST_ADDR;

            GPIO_BOP(GPIOA) = 0x01;
            GPIO_BC(GPIOA) = 0x02;
            //GPIO_BC(GPIOA) = 0x04;
        }

        return;
    }

    // Make sure we are getting what we expected:
    if (EXTI_FTEN & context->scl_pin) {
        // Expecting a falling edge
        if (gpio_input_bit_get(context->gpio, context->scl_pin) != 0)
            return;
    }
    else if (EXTI_RTEN & context->scl_pin) {
        if (gpio_input_bit_get(context->gpio, context->scl_pin) != 1)
            return;
    }

    // Main FSM
    switch (context->state) {
    case ST_IDLE:
        // Start condition handled before
        break;
    case ST_ADDR:
        context->addr |= gpio_input_bit_get(context->gpio, context->sda_pin);
        context->count ++;
        if (context->count == 8) {
            // Address received, match address and send ack
            if (((context->addr & 0xf8) == 0xa0) || ((context->addr & 0xfe) == 0x82)) {
                // Address matched
                context->state = ST_ADDR_ACK;
                // Set to falling edge trigger
                EXTI_RTEN &= ~ context->scl_pin;
                EXTI_FTEN |= context->scl_pin;
            }
            else {
                // Address failed to match
                context->state = ST_WAIT_STOP;
                // Disable interrupt for SCL line
                //EXTI_INTEN &= ~ context->scl_pin;
            }
        }
        else {
            context->addr <<= 1;
        }
        break;
    case ST_ADDR_ACK:
        // Send ACK
        GPIO_BC(context->gpio) = context->sda_pin;
        gpio_init(context->gpio, GPIO_MODE_OUT_OD, GPIO_OSPEED_50MHZ, 
                context->sda_pin);
        if (context->addr & 0x01) {
            // Read
            // Prepare the data, 
            // the next falling edge should give ample time to prepare the data
            // So hold off for now
            context->state = ST_READ_PREPARE;
        }
        else {
            // Write
            context->state = ST_WRITE_PREPARE;
        }
        break;
    case ST_READ_PREPARE:
        context->read_cb(context->bus_id, context->addr,
                &(context->data), &(context->read_last));
        EXTI_INTEN &= ~ context->sda_pin;
        GPIO_BOP(context->gpio) = context->sda_pin;
        gpio_init(context->gpio, GPIO_MODE_OUT_OD, GPIO_OSPEED_50MHZ, 
                context->sda_pin);
        context->state = ST_READ;
        context->count = 0;
        __attribute__((fallthrough));
    case ST_READ:
        if (context->data & 0x80)
            GPIO_BOP(context->gpio) = context->sda_pin;
        else
            GPIO_BC(context->gpio) = context->sda_pin;
        context->count ++;
        context->data <<= 1;
        if (context->count == 8) {
            // Release bus on next falling edge
            context->state = ST_READ_ACK;
        }
        break;
    case ST_READ_ACK:
        gpio_init(context->gpio, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, 
                context->sda_pin);
        if (context->read_last) {
            // NACK, no more bytes to send
            //GPIO_BOP(context->gpio) = context->sda_pin;
            EXTI_INTEN |= context->sda_pin;
            context->state = ST_WAIT_STOP;
        }
        else {
            // ACK, more bytes to send
            //GPIO_BC(context->gpio) = context->sda_pin;
            context->state = ST_READ_PREPARE;
        }
        break;
    case ST_WRITE_PREPARE:
        // At this cycle (triggered by falling edge)
        // The master is supposed to put out data on the data bus
        // But we will wait till the next rising edge to sample it
        gpio_init(context->gpio, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ,
                context->sda_pin);
        EXTI_FTEN &= ~ context->scl_pin;
        EXTI_RTEN |= context->scl_pin;
        context->state = ST_WRITE;
        context->count = 0;
        context->data = 0;
        break;
    case ST_WRITE:
        context->data |= gpio_input_bit_get(context->gpio, context->sda_pin);
        context->count ++;
        if (context->count == 8) {
            // Data reception finished
            context->state = ST_WRITE_ACK;
            // Set to falling edge trigger
            EXTI_RTEN &= ~ context->scl_pin;
            EXTI_FTEN |= context->scl_pin;
        }
        else {
            context->data <<= 1;
        }
        break;
    case ST_WRITE_ACK:
        GPIO_BC(context->gpio) = context->sda_pin;
        gpio_init(context->gpio, GPIO_MODE_OUT_OD, GPIO_OSPEED_50MHZ, 
                context->sda_pin);
        context->write_cb(context->bus_id, context->addr, context->data);
        // Need to wait for a stop or next byte
        context->state = ST_WRITE_PREPARE;
        break;
    case ST_WAIT_STOP:
        // Handled before
        break;
    }

    // Output FSM state via GPIO
    uint32_t state_int = context->state;
    GPIO_BOP(GPIOA) = (state_int & 0x01) ? 0x00000001 : 0x00010000;
    GPIO_BOP(GPIOA) = (state_int & 0x02) ? 0x00000002 : 0x00020000;
    //GPIO_BOP(GPIOA) = (state_int & 0x04) ? 0x00000004 : 0x00040000;
    //GPIO_BOP(GPIOA) = (state_int & 0x08) ? 0x00000008 : 0x00080000;
}
