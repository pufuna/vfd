/* ПЧ-1500 — таблица параметров (единый источник для пульта, Modbus и документации) */
#ifndef PARAMS_H
#define PARAMS_H
#include <stdint.h>
#include "vfd_core.h"

#define PF_STOP 1   /* изменение только при остановленном двигателе */
#define PF_CMD  2   /* параметр-команда: не сохраняется, сбрасывается после исполнения */

typedef struct {
    const char *name_ru, *name_en;
    const char *unit;           /* единица измерения (латиницей, для ЖКИ) */
    const char *opts_ru, *opts_en; /* варианты через '|' для перечислимых параметров, иначе NULL */
    int16_t min, max, def;
    uint8_t dec;                /* число знаков после запятой */
    uint8_t flags;
} param_def_t;

enum {
    P_CMD_SRC, P_REF_SRC, P_FREQ_SET, P_F_MAX, P_F_MIN, P_ACC, P_DEC, P_MODE, P_STOP_MODE, P_F_PWM, P_REV_EN,
    P_M_VOLT, P_M_CUR, P_M_FREQ, P_M_RPM, P_M_COS, P_RS, P_RR, P_LSIG, P_LM, P_I0, P_TUNE,
    P_BOOST, P_SLIP_COMP, P_SPD_KP, P_SPD_TI, P_I_LIMIT, P_OL_TIME, P_BRAKE_RES, P_UV, P_OH,
    P_DCB_TIME, P_DCB_CUR, P_AUTOSTART, P_MB_ADDR, P_MB_BAUD, P_MB_PARITY, P_MB_TIMEOUT, P_LANG,
    P_DI3_FUNC, P_F_FIXED, P_RELAY_FUNC, P_CAL_I, P_CAL_U, P_DT_COMP, P_FACTORY,
    P_TERM_MODE, P_VF_CURVE, P_SCURVE, P_SKIP_F, P_SKIP_BAND, P_ACC2, P_DEC2, P_F_JOG, P_AI_MIN, P_AI_MAX, P_AI_FILT,
    P_PID_MODE, P_PID_SET, P_PID_KP, P_PID_TI, P_PID_TD, P_PID_FMIN, P_SLEEP_F, P_SLEEP_T, P_WAKE,
    P_UL_CUR, P_UL_TIME, P_RESTART_N, P_RESTART_T, P_FAN_MODE, P_DISP, P_SPEED_K, P_LOCK,
    P_COUNT
};

#define PWM_DEAD_TIME 2.5e-6f     /* мёртвое время, с */
#define I_TRIP_SW     21.0f       /* программная защита по мгновенному току, А */
#define OV_TRIP       400.0f      /* В */
#define OV_STALL      375.0f      /* В */

extern const param_def_t param_defs[P_COUNT];
extern int16_t param_val[P_COUNT];
extern const uint32_t mb_bauds[5];

void params_defaults(void);
/* 0 — успех; 2 — нет такого параметра; 3 — вне диапазона; 4 — запрещено при работе */
int  params_set(int idx, int16_t v, int running);
float params_get_f(int idx);
void params_to_cfg(vfd_cfg_t *c);
int  params_motor_valid(void);
void params_from_tune(const vfd_tune_t *t);
/* имя варианта перечислимого параметра (копируется в buf) */
const char *param_opt(int idx, int val, int lang_en, char *buf, int buflen);
#endif
