/* Copyright 2020 Wenting Zhang
 * Released under MIT license 
 */
#include <stdio.h>
#include <stdint.h>
#include "gd32vf103.h"
#include "systick.h"
#include "lcd.h"
#include "ui.h"
#include "fanslave.h"
#include "fanmaster.h"

#define LED_PIN GPIO_PIN_13
#define LED_GPIO_PORT GPIOC
#define LED_GPIO_CLK RCU_GPIOC

void led_init()
{
    rcu_periph_clock_enable(LED_GPIO_CLK);
    gpio_init(LED_GPIO_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, LED_PIN);

    GPIO_BC(LED_GPIO_PORT) = LED_PIN;
}

void led_toggle() {
    static int toggle = 0;
    if (toggle)
        GPIO_BC(LED_GPIO_PORT) = LED_PIN;
    else
        GPIO_BOP(LED_GPIO_PORT) = LED_PIN;
    toggle = !toggle;
}

uint32_t curve_forward(uint32_t val) {
    return (val * 5 - 1000);
}

uint32_t curve_backward(uint32_t val) {
    return (val + 1000) / 5;
}

/*!
    \brief      main function
    \param[in]  none
    \param[out] none
    \retval     none
*/
int main(void)
{
    led_init();

    lcd_init();

    fanmaster_init();

    fanslave_init();

    for (int i = 0; i < 14; i++) {
        fs_actual_tach[i] = 0x0ccc; // set some default placeholder
    }

    ui_init();
    lcd_update();

    while (start_req == 0);
    start_req = 0;
    GPIO_BC(GPIOA) = 0x02;
    fanmaster_start();

    while(1){
        led_toggle();

        if (rpm_update_req == 1) {
            rpm_update_req = 0;
            GPIO_BC(GPIOA) = 0x01;
            
            // Requested RPM
#ifdef LARGE_UI
            uint32_t rpm = 81920 * 60 / fs_requested_tach[0];
            ui_disp_num(13, 0, rpm);
#else
            for (int i = 0; i < 14; i++) {
                uint32_t rpm = 81920 * 60 / fs_requested_tach[i];
                ui_disp_num(0, i * 10 + 13, rpm);
                //ui_disp_hex(0, i * 10 + 13, fs_requested_tach[i]);
            }
#endif

            // Set RPM
            for (int i = 0; i < 14; i++) {
                fm_requested_tach[i] = curve_forward(fs_requested_tach[i]);
            }
            fanmaster_set_tach();
#ifdef LARGE_UI 
            rpm = 81920 * 60 / fm_requested_tach[0];
            ui_disp_num(13, 16, rpm);
#else
            for (int i = 0; i < 14; i++) {
                uint32_t rpm = 81920 * 60 / fm_requested_tach[i];
                ui_disp_num(28, i * 10 + 13, rpm);
            }
#endif

            // Actual RPM
            fanmaster_get_tach();
#ifdef LARGE_UI
            rpm = 81920 * 60 / fm_actual_tach[0];
            ui_disp_num(13, 16, rpm);
#else
            for (int i = 0; i < 14; i++) {
                uint32_t rpm = 81920 * 60 / fm_actual_tach[i];
                ui_disp_num(56, i * 10 + 13, rpm);
                //ui_disp_hex(56, i * 10 + 13, fm_actual_tach[i]);
            }
#endif

            // Report back RPM
            for (int i = 0; i < 14; i++) {
                fs_actual_tach[i] = curve_backward(fm_actual_tach[i]);
            }

            lcd_update();
        }
    }
}
