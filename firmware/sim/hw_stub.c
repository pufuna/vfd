/* Пустые заглушки аппаратного слоя для утилиты dump_params */
#include "hw.h"
uint32_t hw_millis(void) { return 0; }
void hw_pwm_set_freq(float f, float d) { (void)f; (void)d; }
int hw_ipm_fault(void) { return 0; }
void hw_ipm_fault_clear(void) {}
int hw_sensor_fault(void) { return 0; }
void hw_precharge(int on) { (void)on; }
void hw_fan(int on) { (void)on; }
void hw_relay(int on) { (void)on; }
uint8_t hw_keys(void) { return 0; }
uint8_t hw_di(void) { return 0; }
float hw_ai(void) { return 0; }
float hw_temp(void) { return 25; }
void hw_set_cal(float a, float b) { (void)a; (void)b; }
void hw_uart_cfg(int p, uint32_t b, int par) { (void)p; (void)b; (void)par; }
int hw_uart_rx_frame(int p, uint8_t *b, int max) { (void)p; (void)b; (void)max; return 0; }
void hw_uart_tx(int p, const uint8_t *b, int n) { (void)p; (void)b; (void)n; }
int hw_i2c_write(uint8_t a, const uint8_t *d, int n) { (void)a; (void)d; (void)n; return 0; }
int hw_i2c_read(uint8_t a, uint8_t *d, int n) { (void)a; (void)d; (void)n; return 0; }
int hw_flash_load(void *b, int n) { (void)b; (void)n; return -1; }
int hw_flash_save(const void *b, int n, int e) { (void)b; (void)n; (void)e; return 0; }
void hw_delay_ms(uint32_t ms) { (void)ms; }
