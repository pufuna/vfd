/* Modbus RTU slave: функции 03, 04, 06, 16. Переносимая реализация. */
#ifndef MODBUS_H
#define MODBUS_H
#include <stdint.h>

/* Регистры вне таблицы параметров */
#define REG_CTRL      0x0100   /* слово управления: бит0 пуск, бит1 реверс, бит2 сброс аварии */
#define REG_FREQ_REF  0x0101   /* задание частоты, 0,01 Гц */
#define REG_SAVE      0x0102   /* запись 1 — сохранить параметры во флеш */
#define REG_STATUS    0x0200   /* начало блока состояния (только чтение) */
enum { ST_WORD, ST_FAULT, ST_F_OUT, ST_F_REF, ST_CURRENT, ST_VOLT, ST_VDC, ST_TEMP, ST_RPM, ST_TORQUE, ST_POWER,
       ST_DI, ST_AI, ST_OL, ST_FLT1, ST_FLT2, ST_FLT3, ST_FLT4, ST_HOURS, ST_VERSION, ST_STATE, ST_PID_SP, ST_PID_FB, ST_COUNT };

/* Обработчики регистров (реализует приложение). Возврат 0 или код исключения Modbus (2, 3, 4). */
int mb_reg_read(uint16_t addr, uint16_t *val);
int mb_reg_write(uint16_t addr, uint16_t val);

uint16_t mb_crc16(const uint8_t *p, int len);
/* Разбор кадра req[len] (с CRC). Возвращает длину ответа в resp (0 — отвечать не нужно). */
int mb_process(uint8_t own_addr, const uint8_t *req, int len, uint8_t *resp);
#endif
