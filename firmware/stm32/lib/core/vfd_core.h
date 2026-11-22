/*
 * PUFVFD — ядро управления асинхронным двигателем (переносимый C, без привязки к железу).
 *  - скалярное управление U/f (с бустом и компенсацией скольжения)
 *  - бездатчиковое векторное управление (SVC, ориентация по потоку ротора)
 *  - автонастройка (Rs, Rr, Lsigma), торможение постоянным током, защиты
 * Все величины в СИ. Модель двигателя — «обратная Г» (Rs, Lsig, Lm, Rr), эквивалентная звезда.
 */
#ifndef VFD_CORE_H
#define VFD_CORE_H
#include <stdint.h>

typedef enum {
    VFD_ST_STOP = 0, VFD_ST_BOOT, VFD_ST_MAG, VFD_ST_RUN, VFD_ST_DCBRAKE, VFD_ST_TUNE, VFD_ST_FAULT
} vfd_state_t;

enum {
    VFD_F_NONE = 0,
    VFD_F_OC_HW = 1,   /* аппаратная защита силового модуля (КЗ / сверхток)   */
    VFD_F_OC_SW = 2,   /* программная защита по мгновенному току               */
    VFD_F_OV = 3,      /* перенапряжение шины постоянного тока                 */
    VFD_F_UV = 4,      /* пониженное напряжение шины                           */
    VFD_F_OL = 5,      /* перегрузка двигателя (I2t)                           */
    VFD_F_OH = 6,      /* перегрев радиатора                                   */
    VFD_F_COMM = 7,    /* потеря связи Modbus                                  */
    VFD_F_TUNE = 8,    /* ошибка автонастройки                                 */
    VFD_F_EXT = 9,     /* внешняя авария (дискретный вход)                     */
    VFD_F_PRECH = 10,  /* не заряжена шина / неисправна цепь предзаряда        */
    VFD_F_SENSOR = 11, /* неисправность датчиков тока (смещение нуля)          */
    VFD_F_UL = 12,     /* недогрузка (сухой ход насоса, обрыв ремня)           */
    VFD_F_COUNT
};

enum { VFD_MODE_VF = 0, VFD_MODE_SVC = 1 };

typedef struct {
    float ts;            /* период расчёта (= период ШИМ), с                       */
    int   mode;          /* VFD_MODE_*                                              */
    float f_rated;       /* номинальная частота, Гц                                 */
    float v_rated;       /* номинальное линейное напряжение, В (действ.)            */
    float i_rated;       /* номинальный ток, А (действ.)                            */
    float f_slip_rated;  /* номинальное скольжение, Гц                              */
    int   pole_pairs;
    float boost_v;       /* буст на нулевой частоте, В линейное (действ.)           */
    float slip_comp;     /* коэффициент компенсации скольжения U/f, 0..2            */
    float acc_time;      /* время разгона 0 -> f_rated, с                           */
    float dec_time;      /* время торможения f_rated -> 0, с                        */
    int   stop_coast;    /* 1 = останов выбегом                                     */
    float scurve;        /* доля времени разгона на скругление рампы, 0..0.5        */
    int   vf_quad;       /* 1 = квадратичная характеристика U/f (насосы, вентиляторы) */
    float i_limit;       /* токоограничение, А (действ.)                            */
    float i_trip;        /* программная защита по мгновенному току, А (ампл.)       */
    float ov_trip;       /* В                                                       */
    float ov_stall;      /* порог приостановки торможения, В (0 = выкл.)            */
    float uv_trip;       /* В                                                       */
    float dcb_time;      /* время торможения постоянным током, с (0 = выкл.)        */
    float dcb_cur;       /* ток торможения, А (пост.)                               */
    float dt_comp;       /* компенсация мёртвого времени, доля периода (0 = выкл.)  */
    /* параметры двигателя */
    float rs, rr, lsig, lm;
    float i0;            /* ток холостого хода, А (действ.)                         */
    /* вектор */
    float spd_gain;      /* Kp скорости, о.е. (ток q в долях ном. на долю скорости) */
    float spd_ti;        /* Ti скорости, с                                          */
    float cur_bw;        /* полоса контура тока, рад/с                              */
} vfd_cfg_t;

typedef struct { float rs, rr, lsig, lm, i0; } vfd_tune_t;

typedef struct {
    vfd_cfg_t c;
    /* команды (пишет приложение) */
    volatile float   f_target;   /* заданная частота со знаком, Гц */
    volatile uint8_t run_cmd;
    volatile uint8_t tune_req;   /* запрос автонастройки при следующем пуске */
    volatile uint8_t coast_req;  /* 1 = при снятии команды останов выбегом (дискретный вход) */
    /* состояние */
    volatile vfd_state_t state;
    volatile uint8_t fault;
    volatile uint8_t pwm_en;     /* 1 = выходы инвертора разрешены */
    volatile uint8_t tune_done;  /* 1 = результат в tune готов */
    volatile uint8_t limiting;
    vfd_tune_t tune;
    /* мониторинг */
    float f_out, i_rms, v_out, torque, speed_rpm, power, vdc_f;
    float ol_acc;
    /* --- внутреннее --- */
    float f_ramp, ramp_rate, theta, t_state;
    float va_prev, vb_prev;
    float ia, ib;
    float i_int;                 /* интегратор регулятора пост. тока */
    /* вектор */
    float psis_a, psis_b, psir, th_pll, w_pll, we_f, wr_est;
    float int_d, int_q, int_spd, id_ref, iq_ref, fw, iq_f;
    uint8_t low_mode;
    /* автонастройка */
    int   tn_phase; float tn_t, tn_sv, tn_si, tn_v1, tn_i1, tn_amp, tn_ph, tn_a, tn_b, tn_va, tn_vb;
    int   tn_n;
} vfd_t;

void vfd_init(vfd_t *m, const vfd_cfg_t *cfg);
/* Смена конфигурации допускается только в состоянии STOP/FAULT */
void vfd_set_cfg(vfd_t *m, const vfd_cfg_t *cfg);
/* Вызывается каждый период ШИМ. iu, iw — фазные токи (А), vdc — напряжение шины (В).
 * duty[3] — коэффициенты заполнения верхних ключей 0..1 (применяются в следующем периоде). */
void vfd_step(vfd_t *m, float iu, float iw, float vdc, float duty[3]);
/* Медленная функция (вызывать каждые dt секунд, ~10 мс): перегрузка I2t */
void vfd_slow(vfd_t *m, float dt, float ol_time);
void vfd_trip(vfd_t *m, uint8_t fault);
void vfd_reset_fault(vfd_t *m);
void vfd_set_ramp(vfd_t *m, float acc_time, float dec_time);   /* смена времён рампы на ходу */

#endif
