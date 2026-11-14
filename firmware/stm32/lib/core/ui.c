/* Пульт: ЖКИ 20x4 и 5 кнопок (ПУСК, СТОП/ESC, ВВЕРХ, ВНИЗ, ВВОД) */
#include "app.h"
#include "params.h"
#include "hw.h"
#include "lcd.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

enum { SCR_MAIN, SCR_MENU, SCR_EDIT };
static uint8_t scr, keys_prev, rpt_key;
static uint16_t rpt_cnt, frame_cnt;
static int16_t sel, edit_val;

void ui_init(void) { lcd_init(); scr = SCR_MAIN; }

static void fmt_val(char *b, int n, int v, int dec)
{
    static const int p10[4] = {1, 10, 100, 1000};
    int a = v < 0 ? -v : v;
    if (dec) snprintf(b, (size_t)n, "%s%d.%0*d", v < 0 ? "-" : "", a / p10[dec], dec, a % p10[dec]);
    else snprintf(b, (size_t)n, "%d", v);
}

static void param_str(char *b, int n, int idx, int v, int en)
{
    if (param_defs[idx].opts_ru) param_opt(idx, v, en, b, n);
    else { char t[12]; fmt_val(t, sizeof t, v, param_defs[idx].dec); snprintf(b, (size_t)n, "%s %s", t, param_defs[idx].unit); }
}

static int step_for(uint16_t held) { return held > 300 ? 100 : (held > 150 ? 10 : 1); }

static void on_key(uint8_t k, uint16_t held)
{
    int running = app.m.pwm_en;
    if (k == KEY_STOP) {
        app_stop_key();
        if (app.m.state == VFD_ST_FAULT) { hw_ipm_fault_clear(); vfd_reset_fault(&app.m); }
        if (scr == SCR_EDIT) scr = SCR_MENU; else if (scr == SCR_MENU && !running) scr = SCR_MAIN;
        return;
    }
    if (k == KEY_RUN) { if (param_val[P_CMD_SRC] == 0 && !app.stop_latch) app.key_run = 1; return; }
    switch (scr) {
    case SCR_MAIN:
        if (k == KEY_ENTER) { scr = SCR_MENU; break; }
        if (app.pid_on && param_val[P_REF_SRC] != 2 && (k == KEY_UP || k == KEY_DOWN)) {      /* задание ПИД, % */
            int v = param_val[P_PID_SET] + (k == KEY_UP ? 1 : -1) * step_for(held);
            param_val[P_PID_SET] = (int16_t)(v > 1000 ? 1000 : (v < 0 ? 0 : v));
        } else if (!app.pid_on && param_val[P_REF_SRC] == 0 && (k == KEY_UP || k == KEY_DOWN)) {
            int v = param_val[P_FREQ_SET] + (k == KEY_UP ? 1 : -1) * step_for(held);
            int mx = param_val[P_F_MAX];
            int mn = param_val[P_REV_EN] ? -mx : 0;
            if (v > mx) v = mx;
            if (v < mn) v = mn;
            param_val[P_FREQ_SET] = (int16_t)v;
        }
        break;
    case SCR_MENU:
        if (k == KEY_UP) sel = (int16_t)((sel + 1) % P_COUNT);
        else if (k == KEY_DOWN) sel = (int16_t)((sel + P_COUNT - 1) % P_COUNT);
        else if (k == KEY_ENTER && !held) {
            if (!(running && (param_defs[sel].flags & PF_STOP)) && !(param_val[P_LOCK] && sel != P_LOCK)) { edit_val = param_val[sel]; scr = SCR_EDIT; }
        }
        break;
    case SCR_EDIT:
        if (k == KEY_UP || k == KEY_DOWN) {
            int v = edit_val + (k == KEY_UP ? 1 : -1) * step_for(held);
            if (v > param_defs[sel].max) v = param_defs[sel].max;
            if (v < param_defs[sel].min) v = param_defs[sel].min;
            edit_val = (int16_t)v;
        } else if (k == KEY_ENTER && !held) {
            if (params_set(sel, edit_val, running) == 0) { app_param_changed(sel); if (!(param_defs[sel].flags & PF_CMD)) app.save_req = 1; }
            scr = SCR_MENU;
        }
        break;
    }
}

static void draw(void)
{
    char b[48], v[24];
    int en = param_val[P_LANG];
    vfd_t *m = &app.m;
    lcd_begin_frame();
    if (scr == SCR_MAIN && m->state == VFD_ST_FAULT) {
        snprintf(b, sizeof b, "%s  E%02d", en ? "FAULT" : "АВАРИЯ", m->fault);
        lcd_print(0, 0, b);
        lcd_print(1, 0, fault_text(m->fault, en));
        lcd_print(3, 0, en ? "STOP = RESET" : "STOP = СБРОС");
        return;
    }
    if (scr == SCR_MAIN) {
        const char *st;
        if (!app.ready) st = en ? "CHARGE" : "ЗАРЯД";
        else if (app.sleeping) st = en ? "SLEEP" : "СОН";
        else switch (m->state) {
            case VFD_ST_STOP: st = en ? "READY" : "ГОТОВ"; break;
            case VFD_ST_TUNE: st = en ? "TUNING" : "НАСТР."; break;
            case VFD_ST_DCBRAKE: st = en ? "BRAKE" : "ТОРМОЗ"; break;
            default: st = en ? "RUN" : "РАБОТА"; break;
        }
        lcd_print(0, 0, st);
        fmt_val(v, sizeof v, (int)lroundf(m->f_out * 100), 2); snprintf(b, sizeof b, "%7s Hz", v); lcd_print(0, 10, b);
        fmt_val(v, sizeof v, (int)lroundf(m->i_rms * 10), 1);
        snprintf(b, sizeof b, "I=%5sA   U=%3dV", v, (int)lroundf(m->v_out)); lcd_print(1, 0, b);
        if (param_val[P_DISP] == 1) snprintf(b, sizeof b, "n=%5d rpm M=%4d%%", (int)lroundf(m->speed_rpm * params_get_f(P_SPEED_K)), (int)lroundf(m->torque * 100));
        else if (param_val[P_DISP] == 2) snprintf(b, sizeof b, "P=%5dW  OL=%3d%%", (int)lroundf(m->power), (int)lroundf(m->ol_acc * 100));
        else snprintf(b, sizeof b, "Udc=%3dV   t=%3dC", (int)lroundf(m->vdc_f), (int)lroundf(app.temp));
        lcd_print(2, 0, b);
        if (app.pid_on) {
            char w[12];
            fmt_val(v, sizeof v, (int)lroundf(app.pid_sp * 10), 1); fmt_val(w, sizeof w, (int)lroundf(app.pid_fb * 10), 1);
            snprintf(b, sizeof b, "%s%5s%% PV%5s%%", en ? "SET " : "ЗАД.", v, w);
        } else {
            fmt_val(v, sizeof v, (int)lroundf(fabsf(app.f_ref) * 10), 1);
            snprintf(b, sizeof b, "%s%6s Hz  %s", en ? "SET " : "ЗАД.", v, app.f_ref < 0 ? "<<" : ">>");
        }
        lcd_print(3, 0, b);
        if (m->limiting) lcd_print(3, 19, "!");
        return;
    }
    snprintf(b, sizeof b, "P%02d ", sel); lcd_print(0, 0, b);
    lcd_print(0, 4, en ? param_defs[sel].name_en : param_defs[sel].name_ru);
    if (scr == SCR_EDIT) {
        param_str(v, sizeof v, sel, edit_val, en);
        if ((frame_cnt / 3) & 1) lcd_print(1, 0, ">");
        lcd_print(1, 2, v);
        if (!param_defs[sel].opts_ru) {
            char lo[12], hi[12];
            fmt_val(lo, sizeof lo, param_defs[sel].min, param_defs[sel].dec); fmt_val(hi, sizeof hi, param_defs[sel].max, param_defs[sel].dec);
            snprintf(b, sizeof b, "%s ... %s", lo, hi); lcd_print(3, 0, b);
        }
    } else {
        param_str(v, sizeof v, sel, param_val[sel], en);
        lcd_print(1, 2, v);
        if (m->pwm_en && (param_defs[sel].flags & PF_STOP)) lcd_print(3, 0, en ? "LOCKED WHILE RUNNING" : "ТОЛЬКО ПРИ ОСТАНОВЕ");
    }
}

/* вызывается каждые 10 мс */
void ui_task(void)
{
    static uint8_t div;
    uint8_t k = hw_keys(), pressed = (uint8_t)(k & ~keys_prev);
    keys_prev = k;
    for (uint8_t b = 1; b <= KEY_ENTER; b <<= 1) if (pressed & b) { on_key(b, 0); rpt_key = b; rpt_cnt = 0; }
    if (rpt_key && (k & rpt_key) && (rpt_key == KEY_UP || rpt_key == KEY_DOWN)) {   /* автоповтор с ускорением */
        if (++rpt_cnt > 50 && (rpt_cnt % 5) == 0) on_key(rpt_key, rpt_cnt);
    } else if (!(k & rpt_key)) {
        if (rpt_key && rpt_cnt > 50 && scr == SCR_MAIN) app.save_req = 1;   /* сохранить задание после автоповтора */
        rpt_key = 0;
    }
    if (++div >= 20) { div = 0; frame_cnt++; draw(); }
}
