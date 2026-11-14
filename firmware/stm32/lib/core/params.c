#include "params.h"
#include <math.h>
#include <string.h>

static const char NOYES_RU[] = "НЕТ|ДА", NOYES_EN[] = "NO|YES";

const param_def_t param_defs[P_COUNT] = {
/*                 name_ru              name_en             unit   opts_ru / opts_en                                        min    max    def  dec flags */
[P_CMD_SRC]    = {"ИСТОЧНИК КОМАНД",  "COMMAND SOURCE",   "",    "ПУЛЬТ|КЛЕММЫ|MODBUS", "KEYPAD|TERMINALS|MODBUS",          0,     2,     0, 0, PF_STOP},
[P_REF_SRC]    = {"ИСТОЧНИК ЗАДАНИЯ", "REFERENCE SOURCE", "",    "ПУЛЬТ|ПОТЕНЦИОМЕТР|MODBUS", "KEYPAD|ANALOG INPUT|MODBUS",  0,     2,     0, 0, 0},
[P_FREQ_SET]   = {"ЗАДАННАЯ ЧАСТОТА", "FREQ. SETPOINT",   "Hz",  0, 0,                                                   -2000,  2000,   500, 1, 0},
[P_F_MAX]      = {"МАКС. ЧАСТОТА",    "MAX FREQUENCY",    "Hz",  0, 0,                                                      50,  2000,   500, 1, 0},
[P_F_MIN]      = {"МИН. ЧАСТОТА",     "MIN FREQUENCY",    "Hz",  0, 0,                                                       0,   500,     0, 1, 0},
[P_ACC]        = {"ВРЕМЯ РАЗГОНА",    "ACCEL TIME",       "s",   0, 0,                                                       1,  6000,    50, 1, 0},
[P_DEC]        = {"ВРЕМЯ ТОРМОЖЕНИЯ", "DECEL TIME",       "s",   0, 0,                                                       1,  6000,    50, 1, 0},
[P_MODE]       = {"РЕЖИМ УПРАВЛЕНИЯ", "CONTROL MODE",     "",    "U/F|ВЕКТОР", "V/F|VECTOR",                                 0,     1,     0, 0, PF_STOP},
[P_STOP_MODE]  = {"СПОСОБ ОСТАНОВА",  "STOP MODE",        "",    "РАМПА|ВЫБЕГ", "RAMP|COAST",                                0,     1,     0, 0, 0},
[P_F_PWM]      = {"ЧАСТОТА ШИМ",      "PWM FREQUENCY",    "kHz", 0, 0,                                                      40,   120,    80, 1, PF_STOP},
[P_REV_EN]     = {"РАЗРЕШИТЬ РЕВЕРС", "ENABLE REVERSE",   "",    NOYES_RU, NOYES_EN,                                         0,     1,     1, 0, 0},
[P_M_VOLT]     = {"НОМ. НАПРЯЖЕНИЕ",  "RATED VOLTAGE",    "V",   0, 0,                                                     100,   240,   220, 0, PF_STOP},
[P_M_CUR]      = {"НОМ. ТОК",         "RATED CURRENT",    "A",   0, 0,                                                      10,   100,    65, 1, PF_STOP},
[P_M_FREQ]     = {"НОМ. ЧАСТОТА",     "RATED FREQUENCY",  "Hz",  0, 0,                                                     200,  2000,   500, 1, PF_STOP},
[P_M_RPM]      = {"НОМ. СКОРОСТЬ",    "RATED SPEED",      "rpm", 0, 0,                                                     100, 12000,  1400, 0, PF_STOP},
[P_M_COS]      = {"КОСИНУС ФИ",       "POWER FACTOR",     "",    0, 0,                                                      50,    95,    80, 2, PF_STOP},
[P_RS]         = {"R СТАТОРА",        "STATOR R",         "Ohm", 0, 0,                                                       0, 10000,     0, 2, PF_STOP},
[P_RR]         = {"R РОТОРА",         "ROTOR R",          "Ohm", 0, 0,                                                       0, 10000,     0, 2, PF_STOP},
[P_LSIG]       = {"L РАССЕЯНИЯ",      "LEAKAGE L",        "mH",  0, 0,                                                       0, 30000,     0, 1, PF_STOP},
[P_LM]         = {"L НАМАГНИЧИВ.",    "MAGNETIZING L",    "mH",  0, 0,                                                       0, 30000,     0, 1, PF_STOP},
[P_I0]         = {"ТОК ХОЛОСТ. ХОДА", "NO-LOAD CURRENT",  "A",   0, 0,                                                       0,   100,     0, 1, PF_STOP},
[P_TUNE]       = {"АВТОНАСТРОЙКА",    "AUTOTUNE",         "",    "НЕТ|ЗАПУСК", "NO|START",                                   0,     1,     0, 0, PF_STOP | PF_CMD},
[P_BOOST]      = {"БУСТ НАПРЯЖЕНИЯ",  "VOLTAGE BOOST",    "%",   0, 0,                                                       0,   200,    30, 1, 0},
[P_SLIP_COMP]  = {"КОМП. СКОЛЬЖЕНИЯ", "SLIP COMPENS.",    "%",   0, 0,                                                       0,   200,     0, 0, 0},
[P_SPD_KP]     = {"УСИЛЕНИЕ СКОР.",   "SPEED GAIN",       "",    0, 0,                                                       5,   500,    60, 1, 0},
[P_SPD_TI]     = {"ИНТЕГР. СКОРОСТИ", "SPEED INTEG.TIME", "ms",  0, 0,                                                      20,  5000,   200, 0, 0},
[P_I_LIMIT]    = {"ТОКООГРАНИЧЕНИЕ",  "CURRENT LIMIT",    "%",   0, 0,                                                      50,   180,   150, 0, 0},
[P_OL_TIME]    = {"ВРЕМЯ ПЕРЕГРУЗКИ", "OVERLOAD TIME",    "s",   0, 0,                                                      10,   300,    60, 0, 0},
[P_BRAKE_RES]  = {"ТОРМ. РЕЗИСТОР",   "BRAKE RESISTOR",   "",    NOYES_RU, NOYES_EN,                                         0,     1,     0, 0, 0},
[P_UV]         = {"МИН. НАПР. ШИНЫ",  "DC UNDERVOLTAGE",  "V",   0, 0,                                                     150,   280,   200, 0, 0},
[P_OH]         = {"ПЕРЕГРЕВ, ГРАД.",  "OVERTEMP. LIMIT",  "C",   0, 0,                                                      50,   100,    85, 0, 0},
[P_DCB_TIME]   = {"ВРЕМЯ DC-ТОРМ.",   "DC BRAKE TIME",    "s",   0, 0,                                                       0,   300,     0, 1, 0},
[P_DCB_CUR]    = {"ТОК DC-ТОРМОЖ.",   "DC BRAKE CURRENT", "%",   0, 0,                                                      10,   100,    50, 0, 0},
[P_AUTOSTART]  = {"АВТОЗАПУСК",       "AUTO START",       "",    NOYES_RU, NOYES_EN,                                         0,     1,     0, 0, 0},
[P_MB_ADDR]    = {"АДРЕС MODBUS",     "MODBUS ADDRESS",   "",    0, 0,                                                       1,   247,     1, 0, 0},
[P_MB_BAUD]    = {"СКОРОСТЬ MODBUS",  "MODBUS BAUDRATE",  "",    "9600|19200|38400|57600|115200", "9600|19200|38400|57600|115200", 0, 4,  1, 0, 0},
[P_MB_PARITY]  = {"ЧЕТНОСТЬ MODBUS",  "MODBUS PARITY",    "",    "8N1|8E1|8O1", "8N1|8E1|8O1",                               0,     2,     0, 0, 0},
[P_MB_TIMEOUT] = {"ТАЙМАУТ СВЯЗИ",    "COMM TIMEOUT",     "s",   0, 0,                                                       0,   600,     0, 1, 0},
[P_LANG]       = {"ЯЗЫК / LANGUAGE",  "LANGUAGE",         "",    "РУССКИЙ|ENGLISH", "RUSSIAN|ENGLISH",                       0,     1,     0, 0, 0},
[P_DI3_FUNC]   = {"ФУНКЦИЯ ВХОДА 3",  "INPUT 3 FUNCTION", "",    "НЕТ|СБРОС|ВНЕШ. АВАРИЯ|ФИКС. ЧАСТОТА|РЕВЕРС|ТОЛЧОК|РАМПА 2|ПИД ВЫКЛ.|ВЫБЕГ", "NONE|RESET|EXT. FAULT|FIXED FREQ.|REVERSE|JOG|RAMP 2|PID OFF|COAST STOP", 0, 8, 1, 0, 0},
[P_F_FIXED]    = {"ФИКСИР. ЧАСТОТА",  "FIXED FREQUENCY",  "Hz",  0, 0,                                                       0,  2000,   100, 1, 0},
[P_RELAY_FUNC] = {"ФУНКЦИЯ РЕЛЕ",     "RELAY FUNCTION",   "",    "АВАРИЯ|РАБОТА|ГОТОВ|F ДОСТИГНУТА|ОГРАНИЧЕНИЕ|СОН", "FAULT|RUNNING|READY|AT SPEED|LIMITING|SLEEP", 0, 5,  0, 0, 0},
[P_CAL_I]      = {"КАЛИБРОВКА ТОКА",  "CURRENT CALIBR.",  "%",   0, 0,                                                     500,  1500,  1000, 1, 0},
[P_CAL_U]      = {"КАЛИБРОВКА UDC",   "DC VOLT. CALIBR.", "%",   0, 0,                                                     500,  1500,  1000, 1, 0},
[P_DT_COMP]    = {"КОМП. МЕРТВ. ВР.", "DEADTIME COMP.",   "",    NOYES_RU, NOYES_EN,                                         0,     1,     1, 0, PF_STOP},
[P_FACTORY]    = {"ЗАВОДСКОЙ СБРОС",  "FACTORY RESET",    "",    "НЕТ|СБРОС", "NO|RESET",                                    0,     1,     0, 0, PF_STOP | PF_CMD},
[P_TERM_MODE]  = {"РЕЖИМ КЛЕММ",      "TERMINAL MODE",    "",    "ВПЕРЕД/НАЗАД|ПУСК/РЕВЕРС|3-ПРОВОДНЫЙ", "FWD/REV|RUN/DIR|3-WIRE",  0,     2,     0, 0, PF_STOP},
[P_VF_CURVE]   = {"ХАРАКТЕРИСТ. U/F", "V/F CURVE",        "",    "ЛИНЕЙНАЯ|КВАДРАТИЧНАЯ", "LINEAR|QUADRATIC",                0,     1,     0, 0, PF_STOP},
[P_SCURVE]     = {"S-РАМПА",          "S-RAMP",           "%",   0, 0,                                                       0,    50,     0, 0, 0},
[P_SKIP_F]     = {"ЧАСТОТА ПРОПУСКА", "SKIP FREQUENCY",   "Hz",  0, 0,                                                       0,  2000,     0, 1, 0},
[P_SKIP_BAND]  = {"ПОЛОСА ПРОПУСКА",  "SKIP BAND",        "Hz",  0, 0,                                                       0,   100,     0, 1, 0},
[P_ACC2]       = {"ВРЕМЯ РАЗГОНА 2",  "ACCEL TIME 2",     "s",   0, 0,                                                       1,  6000,   100, 1, 0},
[P_DEC2]       = {"ВРЕМЯ ТОРМОЖ. 2",  "DECEL TIME 2",     "s",   0, 0,                                                       1,  6000,   100, 1, 0},
[P_F_JOG]      = {"ЧАСТОТА ТОЛЧКА",   "JOG FREQUENCY",    "Hz",  0, 0,                                                       0,   500,    50, 1, 0},
[P_AI_MIN]     = {"AI НИЖНИЙ ПРЕДЕЛ", "AI LOW LIMIT",     "%",   0, 0,                                                       0,   900,     0, 1, 0},
[P_AI_MAX]     = {"AI ВЕРХНИЙ ПРЕД.", "AI HIGH LIMIT",    "%",   0, 0,                                                     100,  1000,  1000, 1, 0},
[P_AI_FILT]    = {"AI ФИЛЬТР",        "AI FILTER",        "ms",  0, 0,                                                       0,  5000,   100, 0, 0},
[P_PID_MODE]   = {"ПИД РЕЖИМ",        "PID MODE",         "",    "ВЫКЛ.|ПРЯМОЙ|ИНВЕРСНЫЙ", "OFF|DIRECT|REVERSE",              0,     2,     0, 0, PF_STOP},
[P_PID_SET]    = {"ПИД ЗАДАНИЕ",      "PID SETPOINT",     "%",   0, 0,                                                       0,  1000,   500, 1, 0},
[P_PID_KP]     = {"ПИД KP",           "PID KP",           "",    0, 0,                                                       0, 10000,   100, 2, 0},
[P_PID_TI]     = {"ПИД TI",           "PID TI",           "s",   0, 0,                                                       0,  6000,    50, 1, 0},
[P_PID_TD]     = {"ПИД TD",           "PID TD",           "s",   0, 0,                                                       0,  1000,     0, 2, 0},
[P_PID_FMIN]   = {"ПИД МИН. ЧАСТОТА", "PID MIN FREQ.",    "Hz",  0, 0,                                                       0,  2000,   200, 1, 0},
[P_SLEEP_F]    = {"СОН: ЧАСТОТА",     "SLEEP FREQUENCY",  "Hz",  0, 0,                                                       0,  2000,     0, 1, 0},
[P_SLEEP_T]    = {"СОН: ЗАДЕРЖКА",    "SLEEP DELAY",      "s",   0, 0,                                                       1,   600,    30, 0, 0},
[P_WAKE]       = {"СОН: ПРОБУЖДЕНИЕ", "WAKE-UP DEVIAT.",  "%",   0, 0,                                                       1,   500,    50, 1, 0},
[P_UL_CUR]     = {"НЕДОГРУЗКА, ТОК",  "UNDERLOAD LEVEL",  "%",   0, 0,                                                       0,   100,     0, 0, 0},
[P_UL_TIME]    = {"НЕДОГРУЗКА, ВР.",  "UNDERLOAD TIME",   "s",   0, 0,                                                       1,   300,    10, 0, 0},
[P_RESTART_N]  = {"АВТОСБРОС, ЧИСЛО", "AUTO RESET COUNT", "",    0, 0,                                                       0,     5,     0, 0, 0},
[P_RESTART_T]  = {"АВТОСБРОС, ПАУЗА", "AUTO RESET DELAY", "s",   0, 0,                                                       1,   300,    10, 0, 0},
[P_FAN_MODE]   = {"ВЕНТИЛЯТОР",       "COOLING FAN",      "",    "АВТО|ПОСТОЯННО", "AUTO|ALWAYS ON",                         0,     1,     0, 0, 0},
[P_DISP]       = {"СТРОКА 3 ЭКРАНА",  "DISPLAY LINE 3",   "",    "UDC И ТЕМПЕР.|СКОРОСТЬ, МОМЕНТ|МОЩНОСТЬ, ПЕРЕГР.", "UDC, TEMPERATURE|SPEED, TORQUE|POWER, OVERLOAD", 0, 2, 0, 0, 0},
[P_SPEED_K]    = {"КОЭФ. СКОРОСТИ",   "SPEED SCALE",      "",    0, 0,                                                       1, 10000,   100, 2, 0},
[P_LOCK]       = {"ЗАЩИТА ПАРАМЕТР.", "PARAMETER LOCK",   "",    NOYES_RU, NOYES_EN,                                         0,     1,     0, 0, 0},
};

int16_t param_val[P_COUNT];
const uint32_t mb_bauds[5] = {9600, 19200, 38400, 57600, 115200};

void params_defaults(void) { for (int i = 0; i < P_COUNT; i++) param_val[i] = param_defs[i].def; }

int params_set(int idx, int16_t v, int running)
{
    if (idx < 0 || idx >= P_COUNT) return 2;
    const param_def_t *d = &param_defs[idx];
    if (v < d->min || v > d->max) return 3;
    if (running && (d->flags & PF_STOP)) return 4;
    param_val[idx] = v;
    return 0;
}

float params_get_f(int idx)
{
    static const float sc[4] = {1.0f, 0.1f, 0.01f, 0.001f};
    return (float)param_val[idx] * sc[param_defs[idx].dec & 3];
}

int params_motor_valid(void) { return param_val[P_RS] > 0 && param_val[P_RR] > 0 && param_val[P_LSIG] > 0 && param_val[P_LM] > 0; }

void params_to_cfg(vfd_cfg_t *c)
{
    memset(c, 0, sizeof(*c));
    float fpwm = params_get_f(P_F_PWM) * 1000.0f;
    c->ts = 1.0f / fpwm;
    c->mode = param_val[P_MODE];
    c->f_rated = params_get_f(P_M_FREQ);
    c->v_rated = params_get_f(P_M_VOLT);
    c->i_rated = params_get_f(P_M_CUR);
    int pp = (int)(60.0f * c->f_rated / params_get_f(P_M_RPM));   /* округление вниз: 1400 об/мин при 50 Гц -> 2 */
    if (pp < 1) pp = 1;
    if (pp > 8) pp = 8;
    c->pole_pairs = pp;
    c->f_slip_rated = c->f_rated - params_get_f(P_M_RPM) * (float)pp / 60.0f;
    if (c->f_slip_rated < 0.2f) c->f_slip_rated = 0.2f;
    c->boost_v = params_get_f(P_BOOST) * 0.01f * c->v_rated;
    c->slip_comp = params_get_f(P_SLIP_COMP) * 0.01f;
    c->acc_time = params_get_f(P_ACC);
    c->dec_time = params_get_f(P_DEC);
    c->stop_coast = param_val[P_STOP_MODE];
    c->scurve = params_get_f(P_SCURVE) * 0.01f;
    c->vf_quad = param_val[P_VF_CURVE];
    c->i_limit = params_get_f(P_I_LIMIT) * 0.01f * c->i_rated;
    c->i_trip = I_TRIP_SW;
    c->ov_trip = OV_TRIP;
    c->ov_stall = param_val[P_BRAKE_RES] ? 0.0f : OV_STALL;
    c->uv_trip = params_get_f(P_UV);
    c->dcb_time = params_get_f(P_DCB_TIME);
    c->dcb_cur = params_get_f(P_DCB_CUR) * 0.01f * c->i_rated;
    c->dt_comp = param_val[P_DT_COMP] ? (PWM_DEAD_TIME + 0.3e-6f) * fpwm : 0.0f;
    c->rs = params_get_f(P_RS); c->rr = params_get_f(P_RR);
    c->lsig = params_get_f(P_LSIG) * 1e-3f; c->lm = params_get_f(P_LM) * 1e-3f;
    float cosphi = params_get_f(P_M_COS);
    c->i0 = param_val[P_I0] > 0 ? params_get_f(P_I0) : c->i_rated * sqrtf(1.0f - cosphi * cosphi);
    if (c->i0 > 0.9f * c->i_rated) c->i0 = 0.9f * c->i_rated;
    c->spd_gain = params_get_f(P_SPD_KP);
    c->spd_ti = params_get_f(P_SPD_TI) * 1e-3f;
    c->cur_bw = fpwm * 0.19f; if (c->cur_bw > 1500.0f) c->cur_bw = 1500.0f;
}

static int16_t sat16(float v, int16_t max) { return v < 1 ? 1 : (v > max ? max : (int16_t)(v + 0.5f)); }

void params_from_tune(const vfd_tune_t *t)
{
    param_val[P_RS] = sat16(t->rs * 100.0f, 10000);
    param_val[P_RR] = sat16(t->rr * 100.0f, 10000);
    param_val[P_LSIG] = sat16(t->lsig * 1e4f, 30000);
    param_val[P_LM] = sat16(t->lm * 1e4f, 30000);
    param_val[P_I0] = sat16(t->i0 * 10.0f, 100);
    param_val[P_TUNE] = 0;
}

const char *param_opt(int idx, int val, int lang_en, char *buf, int buflen)
{
    const char *s = lang_en ? param_defs[idx].opts_en : param_defs[idx].opts_ru;
    buf[0] = 0;
    if (!s) return buf;
    while (val > 0 && *s) { if (*s == '|') val--; s++; }
    int n = 0;
    while (*s && *s != '|' && n < buflen - 1) buf[n++] = *s++;
    buf[n] = 0;
    return buf;
}
