/* Copyright 2020 Wenting Zhang
 * Released under MIT license 
 * 
 * Driver for ST7735 LCD on Longan Nano board 
 */

#include <stdint.h>
#include "gd32vf103_gpio.h"
#include "gd32vf103_spi.h"
#include "gd32vf103_rcu.h"
#include "systick.h"
#include "lcd.h"

uint16_t framebuffer[LCD_WIDTH * LCD_HEIGHT];

static void lcd_select(void) {
    gpio_bit_reset(GPIOB, GPIO_PIN_2);
}

static void lcd_deselect(void) {
    gpio_bit_set(GPIOB, GPIO_PIN_2);
}

static void lcd_mode_command(void) {
    gpio_bit_reset(GPIOB, GPIO_PIN_0);
}

static void lcd_mode_data(void) {
    gpio_bit_set(GPIOB, GPIO_PIN_0);
}

static void lcd_reset(void) {
    gpio_bit_reset(GPIOB, GPIO_PIN_1);
    delay_1ms(200);
    gpio_bit_set(GPIOB, GPIO_PIN_1);
    delay_1ms(20);
}

// static void lcd_check_busy(void) {
    
// }

static void lcd_send_byte(uint8_t byte) {
    lcd_select();
    while (spi_i2s_flag_get(SPI0, SPI_FLAG_TBE) == RESET);
    spi_i2s_data_transmit(SPI0, byte);
    while (spi_i2s_flag_get(SPI0, SPI_FLAG_RBNE) == RESET);
    spi_i2s_data_receive(SPI0);
    lcd_deselect();
}

static void lcd_send_cmd(uint8_t cmd) {
    lcd_mode_command();
    lcd_send_byte(cmd);
}

static void lcd_send_dat(uint8_t dat) {
    lcd_mode_data();
    lcd_send_byte(dat);
}

static void lcd_send_word(uint16_t word) {
    lcd_mode_data();
    lcd_send_byte(word >> 8);
    lcd_send_byte(word);
}

static void lcd_send_buffer() {
    lcd_mode_data();
    dma_transfer_number_config(DMA0, DMA_CH2, sizeof(framebuffer) - 1);
    lcd_select();
    dma_channel_enable(DMA0, DMA_CH2);
    spi_dma_enable(SPI0, SPI_DMA_TRANSMIT);
    while(!dma_flag_get(DMA0, DMA_CH2, DMA_FLAG_FTF));
    dma_flag_clear(DMA0, DMA_CH2, DMA_FLAG_FTF);
    while (spi_i2s_flag_get(SPI0, SPI_FLAG_TRANS));
    lcd_deselect();
    spi_dma_disable(SPI0, SPI_DMA_TRANSMIT);
    dma_channel_disable(DMA0, DMA_CH2);
}

void lcd_init(void) {
    // Enable clock for GPIO
    rcu_periph_clock_enable(RCU_GPIOA);
	rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_AF);
	rcu_periph_clock_enable(RCU_SPI0);
    rcu_periph_clock_enable(RCU_DMA0);

    // Configure SPI pins
    gpio_init(GPIOA, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_5 |GPIO_PIN_6| GPIO_PIN_7);
	gpio_init(GPIOB, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2);

    // Configure SPI
    spi_parameter_struct spi_init_struct;
    
    spi_struct_para_init(&spi_init_struct);

    /* SPI0 parameter config */
    spi_init_struct.trans_mode           = SPI_TRANSMODE_FULLDUPLEX;
    spi_init_struct.device_mode          = SPI_MASTER;
    spi_init_struct.frame_size           = SPI_FRAMESIZE_8BIT;
    spi_init_struct.clock_polarity_phase = SPI_CK_PL_HIGH_PH_2EDGE;
    spi_init_struct.nss                  = SPI_NSS_SOFT;
    spi_init_struct.prescale             = SPI_PSC_4;
    spi_init_struct.endian               = SPI_ENDIAN_MSB;
    spi_init(SPI0, &spi_init_struct);

    // Set up DMA
    dma_parameter_struct dma_init_struct;

    dma_deinit(DMA0, DMA_CH2);
    dma_struct_para_init(&dma_init_struct);

    dma_init_struct.periph_addr  = (uint32_t)&SPI_DATA(SPI0);
    dma_init_struct.memory_addr  = (uint32_t)framebuffer;
    dma_init_struct.direction    = DMA_MEMORY_TO_PERIPHERAL;
    dma_init_struct.memory_width = DMA_MEMORY_WIDTH_8BIT;
    dma_init_struct.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
    dma_init_struct.priority     = DMA_PRIORITY_LOW;
    dma_init_struct.number       = (uint32_t)sizeof(framebuffer);
    dma_init_struct.periph_inc   = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory_inc   = DMA_MEMORY_INCREASE_ENABLE;
    dma_init(DMA0, DMA_CH2, &dma_init_struct);
 
    dma_circulation_disable(DMA0, DMA_CH2);
    dma_memory_to_memory_disable(DMA0, DMA_CH2);

    // Enable SPI
	spi_crc_polynomial_set(SPI0,7);
	spi_enable(SPI0);

    // Reset LCD
    lcd_deselect();

    lcd_reset();

    // Send initialization sequence 
	lcd_send_cmd(0x11);	// turn off sleep mode
	delay_1ms(100);

	lcd_send_cmd(0x21);	// display inversion mode

	lcd_send_cmd(0xB1);	// Set the frame frequency of the full colors normal mode
						// Frame rate=fosc/((RTNA x 2 + 40) x (LINE + FPA + BPA +2))
						// fosc = 850kHz
	lcd_send_dat(0x05);	// RTNA
	lcd_send_dat(0x3A);	// FPA
	lcd_send_dat(0x3A);	// BPA

	lcd_send_cmd(0xB2);	// Set the frame frequency of the Idle mode
						// Frame rate=fosc/((RTNB x 2 + 40) x (LINE + FPB + BPB +2))
						// fosc = 850kHz
	lcd_send_dat(0x05);	// RTNB
	lcd_send_dat(0x3A);	// FPB
	lcd_send_dat(0x3A);	// BPB

	lcd_send_cmd(0xB3);	// Set the frame frequency of the Partial mode/ full colors
	lcd_send_dat(0x05);  
	lcd_send_dat(0x3A);
	lcd_send_dat(0x3A);
	lcd_send_dat(0x05);
	lcd_send_dat(0x3A);
	lcd_send_dat(0x3A);

	lcd_send_cmd(0xB4);
	lcd_send_dat(0x03);

	lcd_send_cmd(0xC0);
	lcd_send_dat(0x62);
	lcd_send_dat(0x02);
	lcd_send_dat(0x04);

	lcd_send_cmd(0xC1);
	lcd_send_dat(0xC0);

	lcd_send_cmd(0xC2);
	lcd_send_dat(0x0D);
	lcd_send_dat(0x00);

	lcd_send_cmd(0xC3);
	lcd_send_dat(0x8D);
	lcd_send_dat(0x6A);   

	lcd_send_cmd(0xC4);
	lcd_send_dat(0x8D); 
	lcd_send_dat(0xEE); 

	lcd_send_cmd(0xC5);  /*VCOM*/
	lcd_send_dat(0x0E);    

	lcd_send_cmd(0xE0);
	lcd_send_dat(0x10);
	lcd_send_dat(0x0E);
	lcd_send_dat(0x02);
	lcd_send_dat(0x03);
	lcd_send_dat(0x0E);
	lcd_send_dat(0x07);
	lcd_send_dat(0x02);
	lcd_send_dat(0x07);
	lcd_send_dat(0x0A);
	lcd_send_dat(0x12);
	lcd_send_dat(0x27);
	lcd_send_dat(0x37);
	lcd_send_dat(0x00);
	lcd_send_dat(0x0D);
	lcd_send_dat(0x0E);
	lcd_send_dat(0x10);

	lcd_send_cmd(0xE1);
	lcd_send_dat(0x10);
	lcd_send_dat(0x0E);
	lcd_send_dat(0x03);
	lcd_send_dat(0x03);
	lcd_send_dat(0x0F);
	lcd_send_dat(0x06);
	lcd_send_dat(0x02);
	lcd_send_dat(0x08);
	lcd_send_dat(0x0A);
	lcd_send_dat(0x13);
	lcd_send_dat(0x26);
	lcd_send_dat(0x36);
	lcd_send_dat(0x00);
	lcd_send_dat(0x0D);
	lcd_send_dat(0x0E);
	lcd_send_dat(0x10);

	lcd_send_cmd(0x3A);	// define the format of RGB picture data
	lcd_send_dat(0x05);	// 16-bit/pixel

	lcd_send_cmd(0x36);
#ifdef LCD_HORIZONTAL
	lcd_send_dat(0x78); // 0x08 0xc8 0x78 0xa8, direction control
#else
	lcd_send_dat(0x08);
#endif

	lcd_send_cmd(0x29);	// Display On

    lcd_set_window(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);
}

void lcd_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    lcd_send_cmd(0x2a);
    lcd_send_word(x1 + LCD_OFFSET_X);
    lcd_send_word(x2 + LCD_OFFSET_X);
    lcd_send_cmd(0x2b);
    lcd_send_word(y1 + LCD_OFFSET_Y);
    lcd_send_word(y2 + LCD_OFFSET_Y);
    lcd_send_cmd(0x2c);
}

void lcd_update(void) {
    lcd_send_buffer();
}
