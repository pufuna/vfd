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

void app_stop_key(void) { app.key_run = 0; app.stop_latch = 1; }

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
                         (m->limiting ? 32 : 0) | (m->state == VFD_ST_TUNE ? 64 : 0));
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
        if ((val & 4) && !(app.ctrl_word & 4)) { hw_ipm_fault_clear(); vfd_reset_fault(&app.m); }
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
static void logic_10ms(void)
{
    static uint16_t pre_cnt, sec_cnt;
    static uint8_t di_prev, last_state = VFD_ST_STOP;
    vfd_t *m = &app.m;
    uint32_t now = hw_millis();
    app.temp = hw_temp(); app.ai = hw_ai(); app.di = hw_di();

    /* предзаряд: реле шунтирует резистор через 1 с после установления напряжения шины */
    float uv = params_get_f(P_UV);
    if (!app.ready) {
        if (m->vdc_f > uv + 10.0f) { if (++pre_cnt > 100) { app.ready = 1; hw_precharge(1); } } else pre_cnt = 0;
    } else if (m->vdc_f < uv - 40.0f && !m->pwm_en) { app.ready = 0; pre_cnt = 0; hw_precharge(0); }

    /* аварии */
    if (hw_ipm_fault()) vfd_trip(m, VFD_F_OC_HW);
    if (hw_sensor_fault()) vfd_trip(m, VFD_F_SENSOR);
    if (app.temp > params_get_f(P_OH) || app.temp < -40.0f) vfd_trip(m, VFD_F_OH);
    if (param_val[P_DI3_FUNC] == 2 && (app.di & DI_3)) vfd_trip(m, VFD_F_EXT);
    if (param_val[P_DI3_FUNC] == 1 && (app.di & DI_3) && !(di_prev & DI_3)) { hw_ipm_fault_clear(); vfd_reset_fault(m); }
    di_prev = app.di;
    int mb_ctl = (param_val[P_CMD_SRC] == 2 || param_val[P_REF_SRC] == 2);
    if (mb_ctl && param_val[P_MB_TIMEOUT] > 0 && m->pwm_en && (now - app.last_comm_ms) > (uint32_t)param_val[P_MB_TIMEOUT] * 100u)
        vfd_trip(m, VFD_F_COMM);
    vfd_slow(m, 0.01f, params_get_f(P_OL_TIME));
    if (m->state == VFD_ST_FAULT && last_state != VFD_ST_FAULT) { log_fault(m->fault); app.key_run = 0; app.stop_latch = 1; app.ctrl_word &= (uint16_t)~1u; }
    last_state = (uint8_t)m->state;

    /* источник команд */
    int run = 0, rev = 0;
    switch (param_val[P_CMD_SRC]) {
    case 0: run = app.key_run; break;
    case 1: run = ((app.di & DI_FWD) != 0) ^ ((app.di & DI_REV) != 0); rev = (app.di & DI_REV) != 0; break;
    default: run = app.ctrl_word & 1; rev = (app.ctrl_word & 2) != 0; break;
    }
    if (!run) app.stop_latch = 0;                 /* блокировка снимается только после снятия команды пуска */
    if (run && !app.ready && !app.stop_latch && now > 5000 && m->state != VFD_ST_FAULT) vfd_trip(m, VFD_F_PRECH);
    /* источник задания */
    float fmax = params_get_f(P_F_MAX), fmin = params_get_f(P_F_MIN), f;
    switch (param_val[P_REF_SRC]) {
    case 0: f = params_get_f(P_FREQ_SET); break;
    case 1: f = app.ai * fmax; break;
    default: f = (float)app.mb_freq * 0.01f; break;
    }
    if (param_val[P_DI3_FUNC] == 3 && (app.di & DI_3)) f = params_get_f(P_F_FIXED);
    if (rev) f = -f;
    if (f < 0 && !param_val[P_REV_EN]) f = 0;
    float fa = fabsf(f); if (fa > fmax) fa = fmax; if (fa < fmin) fa = fmin;
    app.f_ref = f < 0 ? -fa : fa;
    m->f_target = app.f_ref;

    int tune = param_val[P_TUNE] != 0;
    int allow = app.ready && !app.stop_latch && m->state != VFD_ST_FAULT;
    if (allow && run && !m->pwm_en) {
        if (app.cfg_dirty) apply_cfg();
        if (!tune && param_val[P_MODE] == VFD_MODE_SVC && !params_motor_valid()) { vfd_trip(m, VFD_F_TUNE); allow = 0; }
        m->tune_req = (uint8_t)tune;
    }
    m->run_cmd = (uint8_t)(allow && run);
    if (m->tune_done) {                           /* автонастройка завершена: записать параметры, снять команду */
        m->tune_done = 0; params_from_tune(&m->tune);
        app.key_run = 0; app.stop_latch = 1; app.ctrl_word &= (uint16_t)~1u; app.cfg_dirty = 1; app.save_req = 1;
    }
    if (app.cfg_dirty && !m->pwm_en && m->state != VFD_ST_BOOT) apply_cfg();

    /* вентилятор, реле */
    hw_fan(m->pwm_en || app.temp > 45.0f);
    int rl;
    switch (param_val[P_RELAY_FUNC]) {
    case 0: rl = m->state == VFD_ST_FAULT; break;
    case 1: rl = m->pwm_en; break;
    case 2: rl = app.ready && m->state != VFD_ST_FAULT; break;
    default: rl = m->state == VFD_ST_RUN && fabsf(m->f_ramp - app.f_ref) < 0.05f; break;
    }
    hw_relay(rl);

    if (m->pwm_en && ++sec_cnt >= 100) { sec_cnt = 0; app.run_seconds++; if (app.run_seconds % 3600 == 0) app.save_req = 1; }
    static uint8_t save_div;
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
    "ПЕРЕГРЕВ РАДИАТОРА", "НЕТ СВЯЗИ MODBUS", "ОШИБКА АВТОНАСТРОЙКИ", "ВНЕШНЯЯ АВАРИЯ", "НЕТ ЗАРЯДА ШИНЫ", "ДАТЧИКИ ТОКА" };
static const char *const flt_en[VFD_F_COUNT] = {
    "", "IPM SHORT/OVERCURR.", "OVERCURRENT", "DC BUS OVERVOLTAGE", "DC BUS UNDERVOLTAGE", "MOTOR OVERLOAD",
    "HEATSINK OVERTEMP.", "MODBUS COMM LOST", "AUTOTUNE FAILED", "EXTERNAL FAULT", "DC BUS NOT CHARGED", "CURRENT SENSORS" };
const char *fault_text(int code, int en) { if (code < 0 || code >= VFD_F_COUNT) code = 0; return en ? flt_en[code] : flt_ru[code]; }
