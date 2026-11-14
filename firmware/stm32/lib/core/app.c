#include "app.h"
#include "params.h"
#include "modbus.h"
#include "hw.h"
#include "lcd.h"
#include <string.h>
#include <math.h>

app_t app;

typedef struct { uint32_t magic; int16_t val[P_COUNT]; uint8_t faults[4]; uint32_t run_seconds; uint16_t crc; } store_t;
#define STORE_MAGIC 0x56464431u   /* "VFD1" */

static void store_save(void)
{
    static store_t s;
    memset(&s, 0, sizeof s);
    s.magic = STORE_MAGIC; memcpy(s.val, param_val, sizeof s.val); memcpy(s.faults, app.faults, 4);
    s.val[P_TUNE] = 0; s.val[P_FACTORY] = 0;
    s.run_seconds = app.run_seconds;
    s.crc = mb_crc16((const uint8_t *)&s, (int)((uint8_t *)&s.crc - (uint8_t *)&s));
    if (hw_flash_save(&s, sizeof s, !app.m.pwm_en) != 0) app.save_req = 1;   /* повторить позже (после останова) */
}

static void store_load(void)
{
    static store_t s;
    params_defaults();
    if (hw_flash_load(&s, sizeof s) != 0 || s.magic != STORE_MAGIC) return;
    if (s.crc != mb_crc16((const uint8_t *)&s, (int)((uint8_t *)&s.crc - (uint8_t *)&s))) return;
    for (int i = 0; i < P_COUNT; i++)
        if (s.val[i] >= param_defs[i].min && s.val[i] <= param_defs[i].max && !(param_defs[i].flags & PF_CMD)) param_val[i] = s.val[i];
    memcpy(app.faults, s.faults, 4);
    app.run_seconds = s.run_seconds;
}

static void apply_cfg(void)
{
    vfd_cfg_t c;
    params_to_cfg(&c);
    vfd_set_cfg(&app.m, &c);
    hw_pwm_set_freq(1.0f / c.ts, PWM_DEAD_TIME);
    hw_set_cal(params_get_f(P_CAL_I) * 0.01f, params_get_f(P_CAL_U) * 0.01f);
    app.cfg_dirty = 0;
}

static void apply_comm(void) { hw_uart_cfg(PORT_RS485, mb_bauds[param_val[P_MB_BAUD]], param_val[P_MB_PARITY]); }

void app_param_changed(int idx)
{
    if (idx == P_FACTORY && param_val[P_FACTORY]) { params_defaults(); apply_comm(); app.save_req = 1; }
    if (idx == P_MB_BAUD || idx == P_MB_PARITY) apply_comm();
    if (idx == P_TUNE) return;
    app.cfg_dirty = 1;
}

void app_init(void)
{
    vfd_cfg_t c;
    memset(&app, 0, sizeof app);
    store_load();
    params_to_cfg(&c);
    vfd_init(&app.m, &c);
    apply_cfg(); apply_comm();
    hw_uart_cfg(PORT_LINK, 115200, 0);
    app.stop_latch = param_val[P_AUTOSTART] ? 0 : 1;
    ui_init();
}

int app_isr(float iu, float iw, float vdc, float duty[3])
{
    vfd_step(&app.m, iu, iw, vdc, duty);
    return app.m.pwm_en;
}

void app_stop_key(void) { app.key_run = 0; app.tw_run = 0; app.stop_latch = 1; app.restart_tmr = 0; app.sleeping = 0; }

static void log_fault(uint8_t f)
{
    memmove(&app.faults[1], &app.faults[0], 3); app.faults[0] = f; app.save_req = 1;
}

/* ---------------- регистры Modbus ---------------- */
static uint16_t status_reg(int i)
{
    vfd_t *m = &app.m;
    switch (i) {
    case ST_WORD: return (uint16_t)((app.ready && m->state != VFD_ST_FAULT ? 1 : 0) | (m->pwm_en ? 2 : 0) | (m->f_out < 0 ? 4 : 0) |
                         (m->state == VFD_ST_FAULT ? 8 : 0) | ((m->state == VFD_ST_RUN && fabsf(m->f_ramp - app.f_ref) < 0.05f) ? 16 : 0) |
                         (m->limiting ? 32 : 0) | (m->state == VFD_ST_TUNE ? 64 : 0) | (app.sleeping ? 128 : 0));
    case ST_FAULT:   return m->fault;
    case ST_F_OUT:   return (uint16_t)(int16_t)lroundf(m->f_out * 100);
    case ST_F_REF:   return (uint16_t)(int16_t)lroundf(app.f_ref * 100);
    case ST_CURRENT: return (uint16_t)lroundf(m->i_rms * 100);
    case ST_VOLT:    return (uint16_t)lroundf(m->v_out * 10);
    case ST_VDC:     return (uint16_t)lroundf(m->vdc_f * 10);
    case ST_TEMP:    return (uint16_t)(int16_t)lroundf(app.temp * 10);
    case ST_RPM:     return (uint16_t)(int16_t)lroundf(m->speed_rpm);
    case ST_TORQUE:  return (uint16_t)(int16_t)lroundf(m->torque * 1000);
    case ST_POWER:   return (uint16_t)(int16_t)lroundf(m->power);
    case ST_DI:      return app.di;
    case ST_AI:      return (uint16_t)lroundf(app.ai * 1000);
    case ST_OL:      return (uint16_t)lroundf(m->ol_acc * 100);
    case ST_FLT1: case ST_FLT2: case ST_FLT3: case ST_FLT4: return app.faults[i - ST_FLT1];
    case ST_HOURS:   return (uint16_t)(app.run_seconds / 3600);
    case ST_VERSION: return FW_VERSION;
    case ST_STATE:   return (uint16_t)m->state;
    case ST_PID_SP:  return (uint16_t)lroundf(app.pid_sp * 10);
    case ST_PID_FB:  return (uint16_t)lroundf(app.pid_fb * 10);
    }
    return 0;
}

int mb_reg_read(uint16_t addr, uint16_t *val)
{
    if (addr < P_COUNT) { *val = (uint16_t)param_val[addr]; return 0; }
    if (addr == REG_CTRL) { *val = app.ctrl_word; return 0; }
    if (addr == REG_FREQ_REF) { *val = (uint16_t)app.mb_freq; return 0; }
    if (addr == REG_SAVE) { *val = 0; return 0; }
    if (addr >= REG_STATUS && addr < REG_STATUS + ST_COUNT) { *val = status_reg(addr - REG_STATUS); return 0; }
    return 2;
}

int mb_reg_write(uint16_t addr, uint16_t val)
{
    if (addr < P_COUNT) {
        int e = params_set(addr, (int16_t)val, app.m.pwm_en);
        if (e) return e;
        app_param_changed(addr);
        return 0;
    }
    if (addr == REG_CTRL) {
        if ((val & 4) && !(app.ctrl_word & 4)) { hw_ipm_fault_clear(); vfd_reset_fault(&app.m); app.restart_tmr = 0; }
        app.ctrl_word = val; return 0;
    }
    if (addr == REG_FREQ_REF) { app.mb_freq = (int16_t)val; return 0; }
    if (addr == REG_SAVE) { if (val == 1) app.save_req = 1; return 0; }
    return 2;
}

static void modbus_poll(void)
{
    static uint8_t rx[260], tx[260];
    for (int port = 0; port < 2; port++) {
        int n = hw_uart_rx_frame(port, rx, sizeof rx);
        if (n <= 0) continue;
        int r = mb_process(port == PORT_LINK ? 1 : (uint8_t)param_val[P_MB_ADDR], rx, n, tx);
        if (r > 0 || (n >= 8 && rx[0] == 0)) app.last_comm_ms = hw_millis();
        if (r > 0) hw_uart_tx(port, tx, r);
    }
}

/* ---------------- логика 10 мс ---------------- */
static float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }

/* ПИД-регулятор процесса: обратная связь — аналоговый вход, выход — частота. Возвращает частоту, Гц */
static float pid_step(int running, float fmax)
{
    float kp = params_get_f(P_PID_KP), ti = params_get_f(P_PID_TI), td = params_get_f(P_PID_TD);
    float fmin = params_get_f(P_PID_FMIN), sleep_f = params_get_f(P_SLEEP_F);
    static uint32_t sleep_cnt;
    app.pid_sp = (param_val[P_REF_SRC] == 2) ? (float)app.mb_freq * 0.01f : params_get_f(P_PID_SET);
    app.pid_fb = app.ai * 100.0f;
    float e = app.pid_sp - app.pid_fb;
    if (param_val[P_PID_MODE] == 2) e = -e;
    if (kp < 0.01f) kp = 0.01f;
    float imax = 100.0f / kp;
    if (fmin > fmax) fmin = fmax;
    if (!running) {                                  /* безударный пуск: выход равен минимальной частоте */
        app.pid_i = fmin / fmax * imax; app.pid_d = 0; app.pid_prev = e; sleep_cnt = 0;
    } else {
        if (ti > 0.05f) app.pid_i = clampf(app.pid_i + e * 0.01f / ti, 0.0f, imax); else app.pid_i = 0;
        app.pid_d += 0.2f * ((e - app.pid_prev) * 100.0f - app.pid_d);
        app.pid_prev = e;
    }
    float out = clampf(kp * (e + app.pid_i + td * app.pid_d), 0.0f, 100.0f);
    float f = clampf(out * 0.01f * fmax, fmin, fmax);
    if (sleep_f > 0) {                               /* «сон»: при малом расходе двигатель останавливается */
        if (!app.sleeping) {
            if (running && f <= sleep_f) { if (++sleep_cnt > (uint32_t)param_val[P_SLEEP_T] * 100u) app.sleeping = 1; }
            else sleep_cnt = 0;
        } else if (e > params_get_f(P_WAKE)) { app.sleeping = 0; sleep_cnt = 0; }
    } else app.sleeping = 0;
    return f;
}

static void logic_10ms(void)
{
    static uint16_t pre_cnt, sec_cnt, ul_cnt;
    static uint32_t ok_cnt;
    static uint8_t di_prev, last_state = VFD_ST_STOP, save_div;
    vfd_t *m = &app.m;
    uint32_t now = hw_millis();
    app.temp = hw_temp(); app.di = hw_di();
    int di3 = (app.di & DI_3) != 0, fn = param_val[P_DI3_FUNC];

    /* аналоговый вход: масштабирование и фильтр */
    float lo = params_get_f(P_AI_MIN) * 0.01f, hi = params_get_f(P_AI_MAX) * 0.01f;
    if (hi < lo + 0.01f) hi = lo + 0.01f;
    float sc = clampf((hw_ai() - lo) / (hi - lo), 0.0f, 1.0f);
    app.ai += (param_val[P_AI_FILT] > 10 ? 10.0f / (float)param_val[P_AI_FILT] : 1.0f) * (sc - app.ai);

    /* предзаряд: реле шунтирует резистор через 1 с после установления напряжения шины */
    float uv = params_get_f(P_UV);
    if (!app.ready) {
        if (m->vdc_f > uv + 10.0f) { if (++pre_cnt > 100) { app.ready = 1; hw_precharge(1); } } else pre_cnt = 0;
    } else if (m->vdc_f < uv - 40.0f && !m->pwm_en) { app.ready = 0; pre_cnt = 0; hw_precharge(0); }

    /* аварии */
    if (hw_ipm_fault()) vfd_trip(m, VFD_F_OC_HW);
    if (hw_sensor_fault()) vfd_trip(m, VFD_F_SENSOR);
    if (app.temp > params_get_f(P_OH) || app.temp < -40.0f) vfd_trip(m, VFD_F_OH);
    if (fn == 2 && di3) vfd_trip(m, VFD_F_EXT);
    if (fn == 1 && di3 && !(di_prev & DI_3)) { hw_ipm_fault_clear(); vfd_reset_fault(m); app.restart_tmr = 0; }
    int mb_ctl = (param_val[P_CMD_SRC] == 2 || param_val[P_REF_SRC] == 2);
    if (mb_ctl && param_val[P_MB_TIMEOUT] > 0 && m->pwm_en && (now - app.last_comm_ms) > (uint32_t)param_val[P_MB_TIMEOUT] * 100u)
        vfd_trip(m, VFD_F_COMM);
    vfd_slow(m, 0.01f, params_get_f(P_OL_TIME));
    /* недогрузка: ток ниже порога при работе на установившейся частоте (сухой ход насоса, обрыв ремня) */
    if (param_val[P_UL_CUR] > 0 && m->state == VFD_ST_RUN && fabsf(m->f_ramp) > 5.0f && fabsf(m->f_ramp - app.f_ref) < 0.05f &&
        m->i_rms < params_get_f(P_UL_CUR) * 0.01f * m->c.i_rated) {
        if (++ul_cnt > (uint16_t)param_val[P_UL_TIME] * 100u) vfd_trip(m, VFD_F_UL);
    } else ul_cnt = 0;

    if (m->state == VFD_ST_FAULT && last_state != VFD_ST_FAULT) {
        uint8_t f = m->fault;
        log_fault(f);
        int auto_ok = param_val[P_RESTART_N] > 0 && app.restart_cnt < param_val[P_RESTART_N] &&
                      f != VFD_F_OC_HW && f != VFD_F_TUNE && f != VFD_F_SENSOR && f != VFD_F_EXT && f != VFD_F_PRECH;
        if (auto_ok) app.restart_tmr = (uint16_t)(param_val[P_RESTART_T] * 100);
        else { app.restart_tmr = 0; app.key_run = 0; app.tw_run = 0; app.stop_latch = 1; app.ctrl_word &= (uint16_t)~1u; }
    }
    if (m->state == VFD_ST_FAULT && app.restart_tmr && --app.restart_tmr == 0) {      /* автосброс */
        app.restart_cnt++; hw_ipm_fault_clear(); vfd_reset_fault(m);
    }
    if (m->state == VFD_ST_RUN) { if (++ok_cnt > 60000u) app.restart_cnt = 0; } else ok_cnt = 0;   /* 10 минут без аварий */
    last_state = (uint8_t)m->state;

    /* ---- источник команд ---- */
    int run = 0, rev = 0, fwd_in = (app.di & DI_FWD) != 0, rev_in = (app.di & DI_REV) != 0;
    switch (param_val[P_CMD_SRC]) {
    case 0: run = app.key_run; break;
    case 1:
        switch (param_val[P_TERM_MODE]) {
        case 0: run = fwd_in ^ rev_in; rev = rev_in; break;           /* DI1 — вперёд, DI2 — назад */
        case 1: run = fwd_in; rev = rev_in; break;                    /* DI1 — пуск, DI2 — направление */
        default:                                                      /* 3-проводный: DI1 — кнопка ПУСК (НО), DI2 — кнопка СТОП (НЗ) */
            if (!rev_in) app.tw_run = 0;
            else if (fwd_in && !(di_prev & DI_FWD)) app.tw_run = 1;
            run = app.tw_run; break;
        }
        break;
    default: run = app.ctrl_word & 1; rev = (app.ctrl_word & 2) != 0; break;
    }
    di_prev = app.di;
    if (fn == 4 && di3) rev = !rev;
    int jog = (fn == 5 && di3 && !run);
    if (jog) run = 1;
    m->coast_req = (uint8_t)(fn == 8 && di3);
    if (m->coast_req) { run = 0; app.tw_run = 0; }
    if (!run) app.stop_latch = 0;                 /* блокировка снимается только после снятия команды пуска */
    if (run && !app.ready && !app.stop_latch && now > 5000 && m->state != VFD_ST_FAULT) vfd_trip(m, VFD_F_PRECH);

    /* ---- источник задания ---- */
    float fmax = params_get_f(P_F_MAX), fmin = params_get_f(P_F_MIN), f;
    app.pid_on = (uint8_t)(param_val[P_PID_MODE] != 0 && !(fn == 7 && di3) && !jog);
    if (app.pid_on) f = pid_step(m->state == VFD_ST_RUN && run, fmax);
    else {
        app.sleeping = 0;
        switch (param_val[P_REF_SRC]) {
        case 0: f = params_get_f(P_FREQ_SET); break;
        case 1: f = app.ai * fmax; break;
        default: f = (float)app.mb_freq * 0.01f; break;
        }
        if (fn == 3 && di3) f = params_get_f(P_F_FIXED);
        if (jog) f = params_get_f(P_F_JOG);
    }
    if (rev) f = -f;
    if (f < 0 && !param_val[P_REV_EN]) f = 0;
    float fa = fabsf(f); if (fa > fmax) fa = fmax; if (fa < fmin) fa = fmin;
    float sk = params_get_f(P_SKIP_F), hb = params_get_f(P_SKIP_BAND) * 0.5f;      /* пропуск резонансной частоты */
    if (sk > 0 && hb > 0 && fabsf(fa - sk) < hb) fa = fa < sk ? sk - hb : sk + hb;
    app.f_ref = f < 0 ? -fa : fa;
    m->f_target = app.f_ref;
    if (fn == 6 && di3) vfd_set_ramp(m, params_get_f(P_ACC2), params_get_f(P_DEC2));
    else vfd_set_ramp(m, params_get_f(P_ACC), params_get_f(P_DEC));

    int tune = param_val[P_TUNE] != 0;
    int allow = app.ready && !app.stop_latch && m->state != VFD_ST_FAULT && !app.sleeping;
    if (allow && run && !m->pwm_en) {
        if (app.cfg_dirty) apply_cfg();
        if (!tune && param_val[P_MODE] == VFD_MODE_SVC && !params_motor_valid()) { vfd_trip(m, VFD_F_TUNE); allow = 0; }
        m->tune_req = (uint8_t)tune;
    }
    m->run_cmd = (uint8_t)(allow && run);
    if (m->tune_done) {                           /* автонастройка завершена: записать параметры, снять команду */
        m->tune_done = 0; params_from_tune(&m->tune);
        app.key_run = 0; app.tw_run = 0; app.stop_latch = 1; app.ctrl_word &= (uint16_t)~1u; app.cfg_dirty = 1; app.save_req = 1;
    }
    if (app.cfg_dirty && !m->pwm_en && m->state != VFD_ST_BOOT) apply_cfg();

    /* вентилятор, реле */
    hw_fan(param_val[P_FAN_MODE] || m->pwm_en || app.temp > 45.0f);
    int rl;
    switch (param_val[P_RELAY_FUNC]) {
    case 0: rl = m->state == VFD_ST_FAULT; break;
    case 1: rl = m->pwm_en; break;
    case 2: rl = app.ready && m->state != VFD_ST_FAULT; break;
    case 3: rl = m->state == VFD_ST_RUN && fabsf(m->f_ramp - app.f_ref) < 0.05f; break;
    case 4: rl = m->limiting; break;
    default: rl = app.sleeping; break;
    }
    hw_relay(rl);

    if (m->pwm_en && ++sec_cnt >= 100) { sec_cnt = 0; app.run_seconds++; if (app.run_seconds % 3600 == 0) app.save_req = 1; }
    if (app.save_req && ++save_div >= 20) { save_div = 0; app.save_req = 0; store_save(); }
}

void app_loop(void)
{
    static uint32_t t10;
    uint32_t now = hw_millis();
    modbus_poll();
    if ((uint32_t)(now - t10) >= 10) { t10 = now; logic_10ms(); ui_task(); }
    lcd_task();
}

static const char *const flt_ru[VFD_F_COUNT] = {
    "", "КЗ / СВЕРХТОК МОДУЛЯ", "ПЕРЕГРУЗКА ПО ТОКУ", "ПЕРЕНАПРЯЖЕНИЕ ШИНЫ", "НИЗКОЕ НАПРЯЖЕНИЕ", "ПЕРЕГРУЗКА ДВИГАТЕЛЯ",
    "ПЕРЕГРЕВ РАДИАТОРА", "НЕТ СВЯЗИ MODBUS", "ОШИБКА АВТОНАСТРОЙКИ", "ВНЕШНЯЯ АВАРИЯ", "НЕТ ЗАРЯДА ШИНЫ", "ДАТЧИКИ ТОКА", "НЕДОГРУЗКА / СУХ.ХОД" };
static const char *const flt_en[VFD_F_COUNT] = {
    "", "IPM SHORT/OVERCURR.", "OVERCURRENT", "DC BUS OVERVOLTAGE", "DC BUS UNDERVOLTAGE", "MOTOR OVERLOAD",
    "HEATSINK OVERTEMP.", "MODBUS COMM LOST", "AUTOTUNE FAILED", "EXTERNAL FAULT", "DC BUS NOT CHARGED", "CURRENT SENSORS", "UNDERLOAD / DRY RUN" };
const char *fault_text(int code, int en) { if (code < 0 || code >= VFD_F_COUNT) code = 0; return en ? flt_en[code] : flt_ru[code]; }
