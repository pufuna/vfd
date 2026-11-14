/* Тесты переносимой части прошивки: параметры, знакогенератор ЖКИ, Modbus, прикладная логика */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "app.h"
#include "params.h"
#include "modbus.h"
#include "hw.h"
#include "lcd.h"

static int fails;
#define CHECK(cond, ...) do { if (!(cond)) { printf("  [FAIL] "); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

/* ---- заглушки железа ---- */
static uint32_t t_ms; static uint8_t keys, di, ipm_flt, prech, relay, fan; static float temp = 30, ai;
static uint8_t rxbuf[2][260]; static int rxlen[2]; static uint8_t txbuf[260]; static int txlen;
static uint8_t flash[512]; static int flash_ok, saves;
uint32_t hw_millis(void) { return t_ms; }
void hw_pwm_set_freq(float f, float d) { (void)f; (void)d; }
int hw_ipm_fault(void) { return ipm_flt; }
void hw_ipm_fault_clear(void) { ipm_flt = 0; }
int hw_sensor_fault(void) { return 0; }
void hw_precharge(int on) { prech = (uint8_t)on; }
void hw_fan(int on) { fan = (uint8_t)on; }
void hw_relay(int on) { relay = (uint8_t)on; }
uint8_t hw_keys(void) { return keys; }
uint8_t hw_di(void) { return di; }
float hw_ai(void) { return ai; }
float hw_temp(void) { return temp; }
void hw_set_cal(float a, float b) { (void)a; (void)b; }
void hw_uart_cfg(int p, uint32_t b, int par) { (void)p; (void)b; (void)par; }
int hw_uart_rx_frame(int p, uint8_t *b, int max) { int n = rxlen[p]; (void)max; if (n) { memcpy(b, rxbuf[p], (size_t)n); rxlen[p] = 0; } return n; }
void hw_uart_tx(int p, const uint8_t *b, int n) { (void)p; memcpy(txbuf, b, (size_t)n); txlen = n; }
int hw_i2c_write(uint8_t a, const uint8_t *d, int n) { (void)a; (void)d; (void)n; return 0; }
int hw_flash_load(void *b, int n) { if (!flash_ok) return -1; memcpy(b, flash, (size_t)n); return 0; }
int hw_flash_save(const void *b, int n, int e) { (void)e; memcpy(flash, b, (size_t)n); flash_ok = 1; saves++; return 0; }
void hw_delay_ms(uint32_t ms) { (void)ms; }

static float vdc = 310;
static void run_ms(int ms)
{
    float d[3];
    for (int i = 0; i < ms; i++) {
        for (int k = 0; k < 8; k++) app_isr(0, 0, vdc, d);
        t_ms++; app_loop();
    }
}
static void press(uint8_t k) { keys = k; run_ms(30); keys = 0; run_ms(30); }
static int mb(int port, const uint8_t *pdu, int n)
{
    memcpy(rxbuf[port], pdu, (size_t)n); uint16_t c = mb_crc16(pdu, n);
    rxbuf[port][n] = (uint8_t)c; rxbuf[port][n + 1] = (uint8_t)(c >> 8); rxlen[port] = n + 2; txlen = 0;
    run_ms(2); return txlen;
}
static int wr(uint16_t a, uint16_t v) { uint8_t q[6] = {1, 6, (uint8_t)(a >> 8), (uint8_t)a, (uint8_t)(v >> 8), (uint8_t)v}; mb(0, q, 6); return (txbuf[1] & 0x80) ? txbuf[2] : 0; }
static int rd(uint16_t a) { uint8_t q[6] = {1, 3, (uint8_t)(a >> 8), (uint8_t)a, 0, 1}; mb(0, q, 6); return (txbuf[1] & 0x80) ? -txbuf[2] : (int16_t)((txbuf[3] << 8) | txbuf[4]); }

static void check_screen(const char *what, const char *a, const char *b, const char *c)
{
    uint32_t set = 0; int n = lcd_count_special(a, &set);
    int n2 = b ? lcd_count_special(b, &set) : 0, n3 = c ? lcd_count_special(c, &set) : 0;
    if (n2 > n) n = n2; if (n3 > n) n = n3;
    CHECK(n >= 0 && n2 >= 0 && n3 >= 0, "%s: неподдерживаемый символ в '%s' '%s'", what, a, b ? b : "");
    CHECK(n <= 8, "%s: %d специальных знаков (>8) '%s' '%s'", what, n, a, b ? b : "");
}
static int u8len(const char *s) { int n = 0; while (*s) { if ((*s & 0xC0) != 0x80) n++; s++; } return n; }

int main(void)
{
    char buf[64];
    printf("== параметры и тексты ЖКИ ==\n");
    for (int i = 0; i < P_COUNT; i++) {
        const param_def_t *d = &param_defs[i];
        CHECK(d->name_ru && d->name_en, "P%02d без имени", i);
        CHECK(d->def >= d->min && d->def <= d->max, "P%02d значение по умолчанию вне диапазона", i);
        CHECK(u8len(d->name_ru) <= 16 && u8len(d->name_en) <= 16, "P%02d имя длиннее 16", i);
        if (d->opts_ru) for (int v = d->min; v <= d->max; v++) {
            param_opt(i, v, 0, buf, sizeof buf);
            CHECK(buf[0] && u8len(buf) <= 18, "P%02d вариант %d: '%s'", i, v, buf);
            check_screen("параметр", d->name_ru, buf, (d->flags & PF_STOP) ? "ТОЛЬКО ПРИ ОСТАНОВЕ" : 0);
            param_opt(i, v, 1, buf, sizeof buf); CHECK(buf[0] && strlen(buf) <= 18, "P%02d вариант EN %d", i, v);
        } else check_screen("параметр", d->name_ru, (d->flags & PF_STOP) ? "ТОЛЬКО ПРИ ОСТАНОВЕ" : 0, 0);
    }
    for (int f = 1; f < VFD_F_COUNT; f++) {
        CHECK(u8len(fault_text(f, 0)) <= 20 && strlen(fault_text(f, 1)) <= 20 && fault_text(f, 0)[0], "текст аварии %d", f);
        check_screen("авария", "АВАРИЯ", fault_text(f, 0), "STOP = СБРОС");
    }
    const char *st[] = {"ЗАРЯД", "ГОТОВ", "НАСТР.", "ТОРМОЗ", "РАБОТА"};
    for (int i = 0; i < 5; i++) check_screen("главный экран", st[i], "ЗАД.", "СОН");
    FILE *g = fopen("glyphs.txt", "w");
    for (int i = 0; i < 32; i++) { const uint8_t *p = lcd_glyph(i); int any = 0; for (int r = 0; r < 7; r++) any |= p[r];
        if (!any) continue; fprintf(g, "U+%04X\n", 0x410 + i);
        for (int r = 0; r < 7; r++) { for (int b = 4; b >= 0; b--) fputc(p[r] & (1 << b) ? '#' : '.', g); fputc('\n', g); } fputc('\n', g); }
    fclose(g);

    printf("== Modbus ==\n");
    app_init();
    CHECK(mb_crc16((const uint8_t *)"\x01\x03\x00\x00\x00\x01", 6) == 0x0A84, "CRC16");
    run_ms(1500);
    CHECK(app.ready && prech, "предзаряд: ready=%d реле=%d", app.ready, prech);
    CHECK(rd(P_ACC) == 50, "чтение P05 = %d", rd(P_ACC));
    CHECK(wr(P_ACC, 20) == 0 && param_val[P_ACC] == 20, "запись P05");
    CHECK(wr(P_ACC, 0) == 3, "исключение 3 при записи вне диапазона");
    CHECK(rd(0x0500) == -2, "исключение 2 для несуществующего регистра");
    CHECK(rd(REG_STATUS + ST_VDC) == 3100, "Udc = %d", rd(REG_STATUS + ST_VDC));
    CHECK(rd(REG_STATUS + ST_VERSION) == FW_VERSION, "версия");
    { uint8_t q[6] = {5, 3, 0, 0, 0, 1}; CHECK(mb(0, q, 6) == 0, "чужой адрес — нет ответа"); }
    { uint8_t q[6] = {1, 3, 0, 0, 0, 1}; memcpy(rxbuf[0], q, 6); rxbuf[0][6] = 0; rxbuf[0][7] = 0; rxlen[0] = 8; txlen = 0; run_ms(2); CHECK(txlen == 0, "неверная CRC — нет ответа"); }
    { uint8_t q[11] = {1, 16, 0, P_ACC, 0, 2, 4, 0, 30, 0, 40}; CHECK(mb(0, q, 11) == 8 && param_val[P_ACC] == 30 && param_val[P_DEC] == 40, "функция 16"); }
    { uint8_t q[6] = {1, 4, 2, 0, 0, ST_COUNT}; CHECK(mb(1, q, 6) == 5 + 2 * ST_COUNT, "функция 04 по каналу ESP32"); }

    printf("== прикладная логика ==\n");
    press(KEY_RUN); run_ms(200);
    CHECK(app.m.state == VFD_ST_RUN && app.m.pwm_en && fan, "пуск с пульта: состояние %d", app.m.state);
    CHECK(wr(P_MODE, 1) == 4, "запрет изменения режима при работе");
    run_ms(4000);
    CHECK(fabsf(app.m.f_out - 50) < 0.1f, "выход на 50 Гц: %.2f", app.m.f_out);
    CHECK(rd(REG_STATUS + ST_WORD) & 16, "бит «частота достигнута»");
    press(KEY_STOP); run_ms(5000);
    CHECK(app.m.state == VFD_ST_STOP && !app.m.pwm_en, "останов с пульта");
    /* управление по Modbus */
    wr(P_CMD_SRC, 2); wr(P_REF_SRC, 2); wr(P_MB_TIMEOUT, 10); wr(REG_FREQ_REF, 2500); wr(REG_CTRL, 3); run_ms(100);
    for (int i = 0; i < 40; i++) { rd(REG_STATUS); run_ms(100); }
    CHECK(fabsf(app.m.f_out + 25) < 0.1f, "Modbus: реверс 25 Гц: %.2f", app.m.f_out);
    run_ms(1500);
    CHECK(app.m.state == VFD_ST_FAULT && app.m.fault == VFD_F_COMM, "авария по таймауту связи: %d", app.m.fault);
    CHECK(app.faults[0] == VFD_F_COMM && relay, "журнал аварий и реле аварии");
    wr(REG_CTRL, 4); wr(REG_CTRL, 0); run_ms(50);
    CHECK(app.m.state == VFD_ST_STOP, "сброс аварии по Modbus");
    /* клеммы + аппаратная авария */
    wr(P_CMD_SRC, 1); wr(P_REF_SRC, 1); ai = 0.5f; di = DI_FWD; run_ms(4000);
    CHECK(fabsf(app.m.f_out - 25) < 0.2f, "клеммы + аналоговый вход: %.2f Гц", app.m.f_out);
    ipm_flt = 1; run_ms(20);
    CHECK(app.m.fault == VFD_F_OC_HW && !app.m.pwm_en, "аппаратная защита модуля");
    press(KEY_STOP); run_ms(500);
    CHECK(app.m.state == VFD_ST_STOP && !app.m.pwm_en, "после сброса нет самозапуска при активном входе");
    di = 0; run_ms(50); di = DI_FWD; run_ms(300);
    CHECK(app.m.pwm_en, "повторный пуск после снятия и подачи команды");
    temp = 95; run_ms(30); CHECK(app.m.fault == VFD_F_OH, "перегрев"); temp = 30; di = 0;
    /* вектор без параметров двигателя */
    press(KEY_STOP); wr(P_CMD_SRC, 0); wr(P_MODE, 1); run_ms(50); press(KEY_RUN); run_ms(50);
    CHECK(app.m.fault == VFD_F_TUNE, "вектор без автонастройки запрещён: %d", app.m.fault);
    /* сохранение */
    press(KEY_STOP); wr(P_ACC, 77); wr(REG_SAVE, 1); run_ms(400);
    app_init();
    CHECK(param_val[P_ACC] == 77 && app.faults[0] == VFD_F_TUNE, "сохранение/загрузка параметров и журнала");
    printf("== новые функции ==\n");
    flash_ok = 0; app_init(); run_ms(1500);
    /* клеммы, режим «пуск/реверс» */
    wr(P_CMD_SRC, 1); wr(P_TERM_MODE, 1); wr(P_ACC, 10); wr(P_DEC, 10); di = DI_FWD | DI_REV; run_ms(1500);
    CHECK(fabsf(app.m.f_out + 50) < 0.1f, "клеммы пуск/реверс: %.1f Гц", app.m.f_out);
    di = 0; run_ms(1500); CHECK(app.m.state == VFD_ST_STOP, "останов");
    /* 3-проводный режим */
    wr(P_TERM_MODE, 2); di = DI_REV; run_ms(50); di = DI_REV | DI_FWD; run_ms(50); di = DI_REV; run_ms(1500);
    CHECK(app.m.pwm_en && fabsf(app.m.f_out - 50) < 0.1f, "3-проводный: пуск импульсом, %.1f Гц", app.m.f_out);
    di = 0; run_ms(50); di = DI_REV; run_ms(1500);
    CHECK(app.m.state == VFD_ST_STOP, "3-проводный: стоп размыканием DI2 без самозапуска");
    /* толчок и вторая рампа, выбег */
    wr(P_DI3_FUNC, 5); di = DI_REV | DI_3; run_ms(600);
    CHECK(app.m.pwm_en && fabsf(app.m.f_out - 5) < 0.1f, "толчок: %.1f Гц", app.m.f_out);
    di = DI_REV; run_ms(600); CHECK(!app.m.pwm_en, "толчок снят");
    wr(P_DI3_FUNC, 8); di = DI_REV | DI_FWD; run_ms(50); di = DI_REV; run_ms(1200); di = DI_REV | DI_3; run_ms(30);
    CHECK(!app.m.pwm_en, "останов выбегом по DI3"); di = 0; run_ms(50);
    /* пропуск частоты, S-рампа */
    wr(P_CMD_SRC, 0); wr(P_DI3_FUNC, 0); wr(P_SKIP_F, 250); wr(P_SKIP_BAND, 40); wr(P_FREQ_SET, 240); run_ms(30);
    CHECK(fabsf(app.f_ref - 23) < 0.01f, "пропуск частоты: задание %.1f", app.f_ref);
    wr(P_SKIP_F, 0); wr(P_FREQ_SET, 500); wr(P_SCURVE, 30); wr(P_ACC, 20); press(KEY_RUN);
    float fmx = 0, f1 = 0; for (int i = 0; i < 40; i++) { run_ms(100); if (app.m.f_out > fmx) fmx = app.m.f_out; if (i == 2) f1 = app.m.f_out; }
    CHECK(fabsf(app.m.f_out - 50) < 0.05f && fmx < 50.05f && f1 < 3.0f, "S-рампа: итог %.2f, максимум %.2f, через 0,3 с %.2f Гц", app.m.f_out, fmx, f1);
    press(KEY_STOP); run_ms(2500); wr(P_SCURVE, 0);
    /* ПИД: модель — давление пропорционально частоте с инерцией; затем «сон» */
    wr(P_PID_MODE, 1); wr(P_PID_SET, 600); wr(P_PID_KP, 100); wr(P_PID_TI, 20); wr(P_PID_FMIN, 100); wr(P_AI_FILT, 0); wr(P_ACC, 10);
    run_ms(50); press(KEY_RUN);
    for (int i = 0; i < 3000; i++) { ai += 0.01f * (fabsf(app.m.f_out) / 50.0f * 0.9f - ai); run_ms(10); }
    CHECK(fabsf(app.pid_fb - 60) < 1.5f, "ПИД: обратная связь %.1f %% при задании 60 %%, частота %.1f Гц", app.pid_fb, app.m.f_out);
    wr(P_SLEEP_F, 150); wr(P_SLEEP_T, 2); wr(P_WAKE, 50);
    for (int i = 0; i < 600; i++) { ai += 0.02f * (0.8f - ai); run_ms(10); }     /* расход упал: давление держится само */
    CHECK(app.sleeping && !app.m.pwm_en, "ПИД: переход в сон (sleeping=%d)", app.sleeping);
    for (int i = 0; i < 300; i++) { ai += 0.02f * (0.4f - ai); run_ms(10); }
    CHECK(!app.sleeping && app.m.pwm_en, "ПИД: пробуждение при падении давления");
    press(KEY_STOP); run_ms(1500); wr(P_PID_MODE, 0); wr(P_SLEEP_F, 0); ai = 0;
    /* недогрузка и автосброс */
    wr(P_UL_CUR, 30); wr(P_UL_TIME, 1); wr(P_RESTART_N, 1); wr(P_RESTART_T, 1); wr(P_ACC, 5); press(KEY_RUN); run_ms(2200);
    CHECK(app.m.fault == VFD_F_UL, "недогрузка: авария %d", app.m.fault);
    run_ms(1200);
    CHECK(app.m.state == VFD_ST_RUN, "автосброс: привод снова в работе (состояние %d)", app.m.state);
    run_ms(2500);
    CHECK(app.m.state == VFD_ST_FAULT, "автосброс: число попыток исчерпано");
    press(KEY_STOP); run_ms(100); wr(P_UL_CUR, 0);
    /* защита параметров */
    wr(P_LOCK, 1); keys = KEY_ENTER; run_ms(30); keys = 0; run_ms(30); keys = KEY_ENTER; run_ms(30); keys = 0; run_ms(30);
    keys = KEY_UP; run_ms(30); keys = 0; run_ms(30); keys = KEY_ENTER; run_ms(30); keys = 0; run_ms(30);
    CHECK(param_val[P_CMD_SRC] == 0 && param_val[P_REF_SRC] == 0, "защита параметров от изменения с пульта");

    printf(fails ? "ИТОГ: ОШИБОК %d\n" : "ИТОГ: все проверки пройдены\n", fails);
    return fails ? 1 : 0;
}
