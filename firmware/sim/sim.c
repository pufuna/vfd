/* Симуляция ядра ПЧ-1500 на модели асинхронного двигателя 1,5 кВт (220 В, треугольник -> экв. звезда).
 * Сборка и запуск: make test */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "vfd_core.h"

#define P_PAIRS 2
static const double Rs = 1.67, Rr = 1.27, Lls = 0.006, Llr = 0.006, Lm = 0.100, J = 0.006;
typedef struct { double ia, ib, pa, pb, w; } mot_t;   /* токи статора, поток ротора, мех. скорость */
static double g_te;

static void deriv(const mot_t *s, double va, double vb, double tl, mot_t *d)
{
    double Ls = Lls + Lm, Lr = Llr + Lm, sig = 1 - Lm * Lm / (Ls * Lr), Tr = Lr / Rr, we = P_PAIRS * s->w;
    d->pa = Lm / Tr * s->ia - s->pa / Tr - we * s->pb;
    d->pb = Lm / Tr * s->ib - s->pb / Tr + we * s->pa;
    d->ia = (va - Rs * s->ia - Lm / Lr * d->pa) / (sig * Ls);
    d->ib = (vb - Rs * s->ib - Lm / Lr * d->pb) / (sig * Ls);
    g_te = 1.5 * P_PAIRS * Lm / Lr * (s->pa * s->ib - s->pb * s->ia);
    d->w = (g_te - tl * tanh(s->w / 2.0) - 0.0005 * s->w) / J;
}
static void mot_step(mot_t *s, double va, double vb, double tl, double h)
{
    mot_t k1, k2, k3, k4, t;
#define ADV(k, f) t.ia = s->ia + f * k.ia; t.ib = s->ib + f * k.ib; t.pa = s->pa + f * k.pa; t.pb = s->pb + f * k.pb; t.w = s->w + f * k.w
    deriv(s, va, vb, tl, &k1); ADV(k1, h / 2); deriv(&t, va, vb, tl, &k2); ADV(k2, h / 2);
    deriv(&t, va, vb, tl, &k3); ADV(k3, h); deriv(&t, va, vb, tl, &k4);
    s->ia += h / 6 * (k1.ia + 2 * k2.ia + 2 * k3.ia + k4.ia); s->ib += h / 6 * (k1.ib + 2 * k2.ib + 2 * k3.ib + k4.ib);
    s->pa += h / 6 * (k1.pa + 2 * k2.pa + 2 * k3.pa + k4.pa); s->pb += h / 6 * (k1.pb + 2 * k2.pb + 2 * k3.pb + k4.pb);
    s->w += h / 6 * (k1.w + 2 * k2.w + 2 * k3.w + k4.w);
}

static vfd_t m; static mot_t mot; static float duty[3], duty_next[3];
static double t_now, vdc_nom = 300, i_peak;
static int fails;
#define CHECK(cond, ...) do { printf("  [%s] ", (cond) ? " OK " : "FAIL"); printf(__VA_ARGS__); printf("\n"); if (!(cond)) fails++; } while (0)

static vfd_cfg_t base_cfg(void)
{
    vfd_cfg_t c; memset(&c, 0, sizeof c);
    c.ts = 1.0f / 8000; c.f_rated = 50; c.v_rated = 220; c.i_rated = 6.5f; c.f_slip_rated = 50.0f * (1500 - 1400) / 1500;
    c.pole_pairs = P_PAIRS; c.boost_v = 0.03f * 220; c.acc_time = 2; c.dec_time = 2; c.i_limit = 1.5f * 6.5f;
    c.i_trip = 22; c.ov_trip = 400; c.ov_stall = 0; c.uv_trip = 200; c.i0 = 3.9f;
    c.spd_gain = 6; c.spd_ti = 0.2f; c.cur_bw = 1500; c.dt_comp = 0;
    return c;
}
/* один период ШИМ: выборка токов, расчёт, затем двигатель интегрируется с ранее выданными скважностями */
static void run(double dur, double tl, FILE *log)
{
    int n = (int)(dur / m.c.ts);
    for (int k = 0; k < n; k++) {
        /* пульсации шины 100 Гц + шум датчиков тока */
        double vdc = vdc_nom + 15 * sin(2 * M_PI * 100 * t_now);
        double nz = 0.05 * sin(t_now * 91234.0);
        float iu = (float)(mot.ia + nz), iw = (float)(-0.5 * mot.ia - 0.8660254 * mot.ib - nz);
        vfd_step(&m, iu, iw, (float)vdc, duty_next);
        double va = 0, vb = 0;
        if (m.pwm_en || k == 0) {
            double a = duty[0], b = duty[1], c = duty[2], mean = (a + b + c) / 3;
            va = (a - mean) * vdc; vb = ((b - mean) - (c - mean)) / sqrt(3.0) * vdc;
        }
        if (!m.pwm_en) {            /* инвертор выключен: токи быстро спадают через диоды (упрощённо) */
            va = -20 * mot.ia; vb = -20 * mot.ib;
        }
        for (int i = 0; i < 10; i++) mot_step(&mot, va, vb, tl, m.c.ts / 10);
        memcpy(duty, duty_next, sizeof duty);
        double ip = sqrt(mot.ia * mot.ia + mot.ib * mot.ib); if (ip > i_peak) i_peak = ip;
        t_now += m.c.ts;
        if (log && k % 40 == 0)
            fprintf(log, "%.4f,%.2f,%.2f,%.1f,%.2f,%.2f,%.2f,%d\n", t_now, m.f_out, m.f_ramp, mot.w * 30 / M_PI, m.i_rms, g_te, m.torque, m.state);
    }
}
static double rpm(void) { return mot.w * 30 / M_PI; }

int main(void)
{
    vfd_cfg_t c = base_cfg(); float d[3];
    printf("== 1. Скалярное управление U/f ==\n");
    memset(&mot, 0, sizeof mot); vfd_init(&m, &c); FILE *f = fopen("vf.csv", "w");
    fprintf(f, "t,f_out,f_ramp,rpm,i_rms,te,torque,state\n");
    m.f_target = 50; m.run_cmd = 1; i_peak = 0;
    run(3.0, 0, f);
    CHECK(m.state == VFD_ST_RUN && fabs(rpm() - 1500) < 15, "холостой ход 50 Гц: %.0f об/мин, I=%.2f А, пик %.1f А", rpm(), m.i_rms, i_peak);
    CHECK(fabs(m.v_out - 212) < 12, "выходное напряжение %.0f В (ожидается ~215 В при шине 300 В)", m.v_out);
    run(2.0, 10.0, f);
    CHECK(m.state == VFD_ST_RUN && rpm() > 1380 && rpm() < 1440, "нагрузка 10 Н·м: %.0f об/мин, I=%.2f А", rpm(), m.i_rms);
    m.run_cmd = 0; run(3.0, 10.0, f);
    CHECK(m.state == VFD_ST_STOP && fabs(rpm()) < 30, "останов по рампе: состояние %d, %.0f об/мин", m.state, rpm());
    fclose(f);

    printf("== 2. Пуск U/f под нагрузкой с токоограничением ==\n");
    memset(&mot, 0, sizeof mot); c.acc_time = 0.3f; vfd_init(&m, &c); m.f_target = 50; m.run_cmd = 1; i_peak = 0;
    run(4.0, 8.0, NULL);
    CHECK(m.state == VFD_ST_RUN && rpm() > 1380 && i_peak < 22, "разгон 0,3 с, 8 Н·м: %.0f об/мин, пик тока %.1f А (защита 22 А)", rpm(), i_peak);

    printf("== 3. Автонастройка ==\n");
    double Ls = Lls + Lm, Lr = Llr + Lm, lsig_ref = Ls - Lm * Lm / Lr, rr_ref = Rr * (Lm / Lr) * (Lm / Lr), lm_ref = Lm * Lm / Lr;
    memset(&mot, 0, sizeof mot); c = base_cfg(); vfd_init(&m, &c); m.tune_req = 1; m.run_cmd = 1; i_peak = 0;
    run(8.0, 0, NULL); m.run_cmd = 0; run(0.1, 0, NULL);
    CHECK(m.tune_done && m.state == VFD_ST_STOP, "завершена, пик тока %.1f А, вал повернулся до %.0f об/мин", i_peak, rpm());
    CHECK(fabs(m.tune.rs / Rs - 1) < 0.05, "Rs = %.3f Ом (модель %.3f)", m.tune.rs, Rs);
    CHECK(fabs(m.tune.lsig / lsig_ref - 1) < 0.15, "Lsig = %.2f мГн (модель %.2f)", m.tune.lsig * 1e3, lsig_ref * 1e3);
    CHECK(fabs(m.tune.rr / rr_ref - 1) < 0.25, "Rr = %.3f Ом (модель %.3f)", m.tune.rr, rr_ref);
    CHECK(fabs(m.tune.lm / lm_ref - 1) < 0.25, "Lm = %.1f мГн (модель %.1f, оценка по паспорту)", m.tune.lm * 1e3, lm_ref * 1e3);

    printf("== 4. Бездатчиковое векторное управление (параметры из автонастройки) ==\n");
    c.mode = VFD_MODE_SVC; c.rs = m.tune.rs; c.rr = m.tune.rr; c.lsig = m.tune.lsig; c.lm = m.tune.lm;
    c.acc_time = 1.0f; c.dec_time = 1.0f; c.ov_trip = 1000;
    memset(&mot, 0, sizeof mot); vfd_init(&m, &c); f = fopen("svc.csv", "w");
    fprintf(f, "t,f_out,f_ramp,rpm,i_rms,te,torque,state\n");
    m.f_target = 50; m.run_cmd = 1; i_peak = 0;
    run(3.0, 5.0, f);
    CHECK(m.state == VFD_ST_RUN && fabs(rpm() - 1500) < 20, "пуск с 5 Н·м до 50 Гц: %.0f об/мин (задание 1500), I=%.2f А, пик %.1f А", rpm(), m.i_rms, i_peak);
    double wmin = 1e9; for (int i = 0; i < 20; i++) { run(0.05, 10.0, f); if (rpm() < wmin) wmin = rpm(); }
    CHECK(fabs(rpm() - 1500) < 20 && wmin > 1300, "наброс 10 Н·м: %.0f об/мин, провал до %.0f, момент %.0f %%", rpm(), wmin, m.torque * 100);
    m.f_target = 10; run(2.0, 10.0, f);
    CHECK(fabs(rpm() - 300) < 15, "10 Гц, 10 Н·м: %.0f об/мин (задание 300)", rpm());
    m.f_target = 5; run(2.0, 10.0, f);
    CHECK(fabs(rpm() - 150) < 15, "5 Гц, 10 Н·м: %.0f об/мин (задание 150)", rpm());
    m.f_target = -30; run(3.0, 5.0, f);
    CHECK(m.state == VFD_ST_RUN && fabs(rpm() + 900) < 20, "реверс до -30 Гц: %.0f об/мин (задание -900)", rpm());
    m.f_target = 70; run(4.0, 3.0, f);
    CHECK(m.state == VFD_ST_RUN && fabs(rpm() - 2100) < 40, "ослабление поля 70 Гц: %.0f об/мин (задание 2100)", rpm());
    m.run_cmd = 0; run(3.0, 3.0, f);
    CHECK(m.state == VFD_ST_STOP && fabs(rpm()) < 40, "останов: состояние %d, %.0f об/мин, пик тока за тест %.1f А", m.state, rpm(), i_peak);
    fclose(f);

    printf("== 5. Вектор при ошибке параметров (Rs +20 %%, Rr -30 %%, Lm +20 %%) ==\n");
    c.rs *= 1.2f; c.rr *= 0.7f; c.lm *= 1.2f;
    memset(&mot, 0, sizeof mot); vfd_init(&m, &c); m.f_target = 40; m.run_cmd = 1; i_peak = 0;
    run(3.0, 8.0, NULL);
    CHECK(m.state == VFD_ST_RUN && fabs(rpm() - 1200) < 60, "40 Гц, 8 Н·м: %.0f об/мин (задание 1200), I=%.2f А", rpm(), m.i_rms);

    printf("== 6. Защиты ==\n");
    c = base_cfg(); vfd_init(&m, &c); m.run_cmd = 1;
    for (int i = 0; i < 400; i++) vfd_step(&m, 0, 0, 300, d);
    vfd_step(&m, 30, -15, 300, d);
    CHECK(m.state == VFD_ST_FAULT && m.fault == VFD_F_OC_SW && !m.pwm_en, "программная токовая защита");
    m.run_cmd = 0; vfd_reset_fault(&m); vfd_step(&m, 0, 0, 420, d);
    CHECK(m.fault == VFD_F_OV, "перенапряжение шины");
    vfd_reset_fault(&m); m.i_rms = 1.5f * c.i_rated; float tt = 0; while (m.state != VFD_ST_FAULT && tt < 200) { vfd_slow(&m, 0.01f, 60); m.i_rms = 1.5f * c.i_rated; tt += 0.01f; }
    CHECK(m.fault == VFD_F_OL && fabs(tt - 60) < 1, "перегрузка 150 %% -> отключение через %.0f с", tt);

    printf(fails ? "\nИТОГ: ОШИБОК %d\n" : "\nИТОГ: все проверки пройдены\n", fails);
    return fails ? 1 : 0;
}
