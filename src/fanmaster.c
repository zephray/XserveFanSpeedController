/* Copyright 2020 Wenting Zhang
 * Released under MIT license 
 */
#include <stdlib.h>
#include "gd32vf103_gpio.h"
#include "gd32vf103_i2c.h"
#include "fanmaster.h"

// PB6: I2C0_SCL
// PB7: I2C0_SDA
// PB10: I2C1_SCL
// PB11: I2C1_SDA

#define ENABLED_SENSORS (7)

uint32_t fm_requested_tach[14];
uint32_t fm_actual_tach[14];

void fanmaster_i2c_init(uint32_t i2c) {
    i2c_clock_config(i2c, 100000, I2C_DTCY_2);
    i2c_mode_addr_config(i2c, I2C_I2CMODE_ENABLE, I2C_ADDFORMAT_7BITS, 0x00);
    i2c_enable(i2c);
    i2c_ack_config(i2c, I2C_ACK_ENABLE);
}

void fanmaster_i2c_send(uint32_t i2c, uint8_t addr, uint8_t *buf, uint32_t size) {
    // wait until I2C bus is idle
    while(i2c_flag_get(i2c, I2C_FLAG_I2CBSY));
    // send a start condition to I2C bus
    i2c_start_on_bus(i2c);
    // wait until SBSEND bit is set
    while(!i2c_flag_get(i2c, I2C_FLAG_SBSEND));
    // send slave address to I2C bus
    i2c_master_addressing(i2c, addr << 1, I2C_TRANSMITTER);
    // wait until ADDSEND bit is set
    while(!i2c_flag_get(i2c, I2C_FLAG_ADDSEND));
    // clear ADDSEND bit
    i2c_flag_clear(i2c, I2C_FLAG_ADDSEND);
    // wait until the transmit data buffer is empty
    while(!i2c_flag_get(i2c, I2C_FLAG_TBE));

    for(int i = 0; i < size; i++){
        // data transmission
        i2c_data_transmit(i2c, buf[i]);
        // wait until the TBE bit is set
        while(!i2c_flag_get(i2c, I2C_FLAG_TBE));
    }

    // send a stop condition to I2C bus
    i2c_stop_on_bus(i2c);
    // wait until stop condition generate
    while(I2C_CTL0(i2c)&0x0200);
}

void fanmaster_i2c_read(uint32_t i2c, uint8_t addr, uint8_t reg, uint8_t *buf, uint32_t size) {
    // wait until I2C bus is idle
    while(i2c_flag_get(i2c, I2C_FLAG_I2CBSY));
    // send a start condition to I2C bus
    i2c_start_on_bus(i2c);
    // wait until SBSEND bit is set
    while(!i2c_flag_get(i2c, I2C_FLAG_SBSEND));
    // send slave address to I2C bus
    i2c_master_addressing(i2c, addr << 1, I2C_TRANSMITTER);
    // wait until ADDSEND bit is set
    while(!i2c_flag_get(i2c, I2C_FLAG_ADDSEND));
    // clear ADDSEND bit
    i2c_flag_clear(i2c, I2C_FLAG_ADDSEND);
    // wait until the transmit data buffer is empty
    while(!i2c_flag_get(i2c, I2C_FLAG_TBE));

    // send register index
    i2c_data_transmit(i2c, reg);
    // wait until the TBE bit is set
    while(!i2c_flag_get(i2c, I2C_FLAG_TBE));

    // send a repeated start
    i2c_start_on_bus(i2c);

    // wait until SBSEND bit is set
    while(!i2c_flag_get(i2c, I2C_FLAG_SBSEND));
    // send slave address to I2C bus
    i2c_master_addressing(i2c, addr << 1, I2C_RECEIVER);
    // wait until ADDSEND bit is set
    while(!i2c_flag_get(i2c, I2C_FLAG_ADDSEND));
    // clear ADDSEND bit
    i2c_flag_clear(i2c, I2C_FLAG_ADDSEND);
    // Wait until the last data byte is received into the shift register
    while(!i2c_flag_get(i2c, I2C_FLAG_BTC));

    for (int i = 0; i < size; i++) {
        // wait until the RBNE bit is set
        while(!i2c_flag_get(i2c, I2C_FLAG_RBNE));
        // read a data from I2C_DATA
        buf[i] = i2c_data_receive(i2c);
        if (i == size - 2) {
            // disable ACK before clearing ADDSEND bit
            i2c_ack_config(i2c, I2C_ACK_DISABLE);
        }
    }

    // send a stop condition
    i2c_stop_on_bus(i2c);
    // wait until stop condition generate
    while(I2C_CTL0(i2c)&0x0200);
    i2c_ackpos_config(i2c, I2C_ACKPOS_CURRENT);
    // enable acknowledge
    i2c_ack_config(i2c, I2C_ACK_ENABLE);
}

void fanmaster_init(void) {
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_I2C0);
    rcu_periph_clock_enable(RCU_I2C1);
    
    gpio_init(GPIOB, GPIO_MODE_AF_OD, GPIO_OSPEED_50MHZ, GPIO_PIN_6 | GPIO_PIN_7);
    gpio_init(GPIOB, GPIO_MODE_AF_OD, GPIO_OSPEED_50MHZ, GPIO_PIN_10 | GPIO_PIN_11);

    fanmaster_i2c_init(I2C0);
    fanmaster_i2c_init(I2C1);
}

void fanmaster_start(void) {
    uint8_t sendbuf[4];

    sendbuf[0] = 0x00;
    fanmaster_i2c_send(I2C1, 0x41, sendbuf, 1);

    for (int i = 0; i < ENABLED_SENSORS; i++) {
        uint32_t i2c = (i < 4) ? I2C0 : I2C1;
        uint32_t addr = 3 - i % 4 + 0x50;

        // Reset
        sendbuf[0] = 0x02;
        sendbuf[1] = 0x01;
        fanmaster_i2c_send(i2c, addr, sendbuf, 2);

        // Set Read count
        sendbuf[0] = 0x00;
        sendbuf[1] = 0x02;
        fanmaster_i2c_send(i2c, addr, sendbuf, 2);

        // Set mode
        sendbuf[0] = 0x01;
        sendbuf[1] = 0x01;
        fanmaster_i2c_send(i2c, addr, sendbuf, 2);

        sendbuf[0] = 0x03;
        sendbuf[1] = 0x44;
        fanmaster_i2c_send(i2c, addr, sendbuf, 2);

        sendbuf[0] = 0x3c;
        sendbuf[1] = 0x33;
        fanmaster_i2c_send(i2c, addr, sendbuf, 2);
    }
    
    // Set TACH to 1500RPM
    for (int i = 0; i < ENABLED_SENSORS * 2; i++) {
        fm_requested_tach[i] = 0x0ccc;
    }

    fanmaster_set_tach();
}

void fanmaster_set_tach(void) {
    uint8_t sendbuf[4];

    for (int i = 0; i < ENABLED_SENSORS; i++) {
        uint32_t i2c = (i < 4) ? I2C0 : I2C1;
        uint32_t addr = 3 - i % 4 + 0x50;

        sendbuf[0] = 0xaa;
        sendbuf[1] = 0x02;
        sendbuf[2] = fm_requested_tach[i * 2] & 0xff;
        sendbuf[3] = (fm_requested_tach[i * 2] >> 8) & 0xff;

        fanmaster_i2c_send(i2c, addr, sendbuf, 4);

        sendbuf[0] = 0xac;
        sendbuf[2] = fm_requested_tach[i * 2 + 1] & 0xff;
        sendbuf[3] = (fm_requested_tach[i * 2 + 1] >> 8) & 0xff;

        fanmaster_i2c_send(i2c, addr, sendbuf, 4);
    }
}

void fanmaster_get_tach(void) {
    uint8_t recvbuf[3];

    for (int i = 0; i < ENABLED_SENSORS; i++) {
        uint32_t i2c = (i < 4) ? I2C0 : I2C1;
        uint32_t addr = 3 - i % 4 + 0x50;

        fanmaster_i2c_read(i2c, addr, 0xca, recvbuf, 3);
        fm_actual_tach[i * 2] = 
                ((recvbuf[1] & 0xff) | ((recvbuf[2] << 8) & 0xff00));

        fanmaster_i2c_read(i2c, addr, 0xcc, recvbuf, 3);
        fm_actual_tach[i * 2 + 1] = 
                ((recvbuf[1] & 0xff) | ((recvbuf[2] << 8) & 0xff00));
    }
}