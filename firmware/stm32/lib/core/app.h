/* PUFVFD — прикладная логика (переносимая): команды, защиты, регистры Modbus, хранение параметров */
#ifndef APP_H
#define APP_H
#include <stdint.h>
#include "vfd_core.h"

#define FW_VERSION 110      /* 1.10 */

typedef struct {
    vfd_t   m;
    uint8_t ready;          /* шина заряжена, реле предзаряда включено */
    uint8_t key_run;        /* команда «пуск» с пульта */
    uint8_t stop_latch;     /* блокировка после кнопки СТОП / аварии до снятия команды пуска */
    uint8_t cfg_dirty, save_req;
    uint16_t ctrl_word;     /* слово управления Modbus */
    int16_t  mb_freq;       /* задание Modbus, 0,01 Гц */
    float   f_ref;          /* действующее задание со знаком, Гц */
    float   temp, ai;
    uint8_t di;
    uint8_t faults[4];      /* журнал последних аварий */
    uint8_t tw_run;         /* защёлка пуска в 3-проводном режиме клемм */
    uint8_t sleeping;       /* ПИД: режим «сон» */
    uint8_t pid_on;
    float   pid_sp, pid_fb, pid_i, pid_d, pid_prev;   /* ПИД-регулятор процесса, % */
    uint8_t restart_cnt; uint16_t restart_tmr;        /* автосброс аварий */
    uint32_t run_seconds;
    uint32_t last_comm_ms;
} app_t;

extern app_t app;

void app_init(void);
void app_loop(void);                 /* вызывать непрерывно из main() */
/* вызывается из прерывания АЦП каждый период ШИМ; возвращает 1, если выходы должны быть разрешены */
int  app_isr(float iu, float iw, float vdc, float duty[3]);
void app_param_changed(int idx);     /* после успешного params_set() */
void app_stop_key(void);
void ui_init(void);
void ui_task(void);
const char *fault_text(int code, int lang_en);
#endif
