/* Аппаратный интерфейс PUFVFD (реализация: src/hw_stm32.c; в тестах — заглушки) */
#ifndef HW_H
#define HW_H
#include <stdint.h>

#define KEY_RUN   0x01
#define KEY_STOP  0x02   /* СТОП / СБРОС / ESC */
#define KEY_UP    0x04
#define KEY_DOWN  0x08
#define KEY_ENTER 0x10
#define DI_FWD    0x01
#define DI_REV    0x02
#define DI_3      0x04
#define PORT_RS485 0
#define PORT_LINK  1     /* связь с модулем ESP32 (Wi-Fi / Bluetooth) */

uint32_t hw_millis(void);
void  hw_pwm_set_freq(float f_pwm, float dead_time);   /* только при выключенном ШИМ */
int   hw_ipm_fault(void);        /* 1 — сработала аппаратная защита (вход BKIN) */
void  hw_ipm_fault_clear(void);
int   hw_sensor_fault(void);     /* 1 — смещение нуля датчиков тока вне допуска */
void  hw_precharge(int on);
void  hw_fan(int on);
void  hw_relay(int on);
uint8_t hw_keys(void);
uint8_t hw_di(void);
float hw_ai(void);               /* 0..1 */
float hw_temp(void);             /* температура радиатора, °C */
void  hw_set_cal(float k_i, float k_u);
void  hw_uart_cfg(int port, uint32_t baud, int parity);
int   hw_uart_rx_frame(int port, uint8_t *buf, int max);  /* длина принятого кадра или 0 */
void  hw_uart_tx(int port, const uint8_t *buf, int len);
int   hw_i2c_write(uint8_t addr, const uint8_t *data, int len);  /* 0 — успех */
int   hw_i2c_read(uint8_t addr, uint8_t *data, int len);
int   hw_flash_load(void *buf, int len);                  /* 0 — успех */
int   hw_flash_save(const void *buf, int len, int may_erase);
void  hw_delay_ms(uint32_t ms);
#endif
