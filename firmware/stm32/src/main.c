/* PUFVFD — точка входа прошивки платы управления (STM32F411) */
#include "stm32f4xx.h"
#include "app.h"
#include "hw.h"

void hw_init(void);
void hw_start(void);
void hw_led(int on);

int main(void)
{
    hw_init();
    app_init();
    hw_start();                 /* разрешить прерывание контура управления */
    for (;;) {
        IWDG->KR = 0xAAAA;
        app_loop();
        /* светодиод: редкие вспышки — готов, частые — работа, постоянно — авария */
        uint32_t t = hw_millis();
        hw_led(app.m.state == VFD_ST_FAULT ? 1 : (app.m.pwm_en ? (t & 128) != 0 : (t & 1023) < 60));
    }
}
