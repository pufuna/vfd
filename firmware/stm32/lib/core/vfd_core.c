/* ПЧ-1500 — ядро управления. См. vfd_core.h */
#include "vfd_core.h"
#include <math.h>
#include <string.h>

#define PI_F      3.14159265f
#define TWO_PI    6.28318531f
#define SQRT3     1.73205081f
#define SQRT2     1.41421356f
#define T_BOOT    0.02f     /* заряд бутстрепных конденсаторов, с */
#define F_STOP    0.5f      /* частота, ниже которой останов считается завершённым, Гц */
#define F_SW_LO   2.5f      /* SVC: переход к разомкнутому формированию угла, Гц */
#define F_SW_HI   3.5f      /* SVC: переход к наблюдателю потока, Гц */
#define LAMBDA    0.3f      /* SVC: относительная частота среза интегратора потока */
#define TUNE_F    100.0f    /* частота тестового сигнала автонастройки, Гц */

static float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
static float wrap_pi(float a) { while (a > PI_F) a -= TWO_PI; while (a < -PI_F) a += TWO_PI; return a; }
static float sgnf(float x) { return x < 0 ? -1.0f : 1.0f; }

void vfd_set_cfg(vfd_t *m, const vfd_cfg_t *cfg) { m->c = *cfg; }

void vfd_init(vfd_t *m, const vfd_cfg_t *cfg)
{
    memset(m, 0, sizeof(*m));
    m->c = *cfg;
    m->state = VFD_ST_STOP;
}

void vfd_trip(vfd_t *m, uint8_t fault)
{
    m->pwm_en = 0;
    if (m->state != VFD_ST_FAULT) { m->fault = fault; m->state = VFD_ST_FAULT; }
    m->f_ramp = 0; m->f_out = 0;
}

void vfd_reset_fault(vfd_t *m)
{
    if (m->state == VFD_ST_FAULT && !m->run_cmd) { m->fault = VFD_F_NONE; m->state = VFD_ST_STOP; m->ol_acc = 0; }
}

static void enter(vfd_t *m, vfd_state_t s) { m->state = s; m->t_state = 0; }

static void reset_ctrl(vfd_t *m)
{
    m->f_ramp = 0; m->theta = 0; m->i_int = 0;
    m->psis_a = m->psis_b = 0; m->psir = 0; m->th_pll = 0; m->w_pll = 0; m->we_f = 0; m->wr_est = 0;
    m->int_d = m->int_q = m->int_spd = 0; m->id_ref = m->iq_ref = 0; m->low_mode = 1; m->fw = 1; m->iq_f = 0;
    m->va_prev = m->vb_prev = 0;
}

/* --- пространственно-векторная ШИМ (инжекция нулевой последовательности min-max) --- */
static void svpwm(vfd_t *m, float va, float vb, float vdc, float iu, float iv, float iw, float duty[3])
{
    float vmax = vdc * (0.98f / SQRT3);
    float mag = sqrtf(va * va + vb * vb);
    if (mag > vmax) { va *= vmax / mag; vb *= vmax / mag; }
    m->va_prev = va; m->vb_prev = vb;
    float k = 1.0f / (vdc > 50.0f ? vdc : 50.0f);
    float a = va * k, b = (-0.5f * va + 0.5f * SQRT3 * vb) * k, c = (-0.5f * va - 0.5f * SQRT3 * vb) * k;
    float mx = a > b ? (a > c ? a : c) : (b > c ? b : c);
    float mn = a < b ? (a < c ? a : c) : (b < c ? b : c);
    float off = 0.5f - 0.5f * (mx + mn);
    float ith = 0.05f * SQRT2 * m->c.i_rated + 0.05f;
    float dt = m->c.dt_comp;
    duty[0] = clampf(a + off + dt * clampf(iu / ith, -1, 1), 0, 1);
    duty[1] = clampf(b + off + dt * clampf(iv / ith, -1, 1), 0, 1);
    duty[2] = clampf(c + off + dt * clampf(iw / ith, -1, 1), 0, 1);
}

/* --- задатчик интенсивности с приостановкой по току и напряжению шины --- */
static void ramp(vfd_t *m, float vdc)
{
    const vfd_cfg_t *c = &m->c;
    float tgt = m->run_cmd ? m->f_target : 0.0f;
    float up = c->f_rated / (c->acc_time > 0.05f ? c->acc_time : 0.05f) * c->ts;
    float dn = c->f_rated / (c->dec_time > 0.05f ? c->dec_time : 0.05f) * c->ts;
    float f = m->f_ramp, d = tgt - f;
    int accel = (fabsf(tgt) > fabsf(f)) && (tgt * f >= 0);
    m->limiting = 0;
    if (accel) {
        if (c->mode == VFD_MODE_VF && m->i_rms > c->i_limit) { m->limiting = 1; return; }
        f += clampf(d, -up, up);
    } else {
        if (c->ov_stall > 0 && vdc > c->ov_stall) { m->limiting = 1; return; }
        f += clampf(d, -dn, dn);
    }
    m->f_ramp = f;
}

/* --- интегральный регулятор постоянного тока по оси alpha (не требует параметров двигателя) --- */
static float dc_ctrl(vfd_t *m, float i_ref)
{
    float rbase = m->c.v_rated / (SQRT3 * m->c.i_rated);
    m->i_int += 2.0f * rbase * (i_ref - m->ia) * m->c.ts;
    m->i_int = clampf(m->i_int, 0.0f, 0.35f * m->c.v_rated);
    return m->i_int;
}

/* --- U/f --- */
static void run_vf(vfd_t *m, float *va, float *vb)
{
    const vfd_cfg_t *c = &m->c;
    float f = m->f_ramp;
    /* активная составляющая тока -> компенсация скольжения */
    float vm = sqrtf(m->va_prev * m->va_prev + m->vb_prev * m->vb_prev);
    float iact = vm > 1.0f ? (m->va_prev * m->ia + m->vb_prev * m->ib) / vm : 0.0f;
    float irat = SQRT2 * c->i_rated * 0.8f;
    m->torque += 0.002f * (iact / irat - m->torque);
    float fs = f + sgnf(f) * c->slip_comp * c->f_slip_rated * clampf(m->torque, -1.5f, 1.5f);
    float fa = fabsf(fs);
    float vll = fa >= c->f_rated ? c->v_rated : c->boost_v + (c->v_rated - c->boost_v) * fa / c->f_rated;
    float amp = vll * (SQRT2 / SQRT3);
    m->theta = wrap_pi(m->theta + TWO_PI * fs * c->ts);
    *va = amp * cosf(m->theta); *vb = amp * sinf(m->theta);
    m->f_out = fs;
    m->speed_rpm = (f - (c->slip_comp > 0 ? 0 : sgnf(f) * c->f_slip_rated * clampf(m->torque, -1.5f, 1.5f)))
                   * 60.0f / (float)c->pole_pairs;
}

/* --- бездатчиковое векторное управление --- */
static void run_svc(vfd_t *m, float vdc, int magnetize, float *va, float *vb)
{
    const vfd_cfg_t *c = &m->c;
    float ts = c->ts, ia = m->ia, ib = m->ib;
    float id_nom = SQRT2 * c->i0;
    float psi_nom = c->lm * id_nom;
    float iq_nom = SQRT2 * sqrtf(fabsf(c->i_rated * c->i_rated - c->i0 * c->i0));
    float w_rated = TWO_PI * c->f_rated;

    /* наблюдатель потока статора: ФНЧ с частотой среза LAMBDA*|we| и точной компенсацией на частоте we */
    float we = fabsf(m->we_f) > TWO_PI ? m->we_f : TWO_PI * sgnf(m->we_f);
    float wc = LAMBDA * fabsf(we);
    m->psis_a += ts * (m->va_prev - c->rs * ia - wc * m->psis_a);
    m->psis_b += ts * (m->vb_prev - c->rs * ib - wc * m->psis_b);
    float l = LAMBDA * sgnf(we);
    float psa = m->psis_a + l * m->psis_b, psb = m->psis_b - l * m->psis_a;
    float pra = psa - c->lsig * ia, prb = psb - c->lsig * ib;
    float psir = sqrtf(pra * pra + prb * prb);
    float th_est = atan2f(prb, pra);
    /* ФАПЧ по углу потока -> синхронная частота */
    float e = wrap_pi(th_est - m->th_pll);
    const float pll_w = 150.0f;
    m->w_pll += pll_w * pll_w * e * ts;
    m->th_pll = wrap_pi(m->th_pll + (m->w_pll + 2.0f * pll_w * e) * ts);

    /* потокоослабление: поток снижается интегральным регулятором запаса по напряжению (см. ниже) */
    float vmax = vdc * (0.98f / SQRT3);
    float psi_ref = psi_nom * m->fw;
    float id_ref = psi_ref / c->lm;
    float ilim = SQRT2 * c->i_limit;
    float iq_max = sqrtf(fabsf(ilim * ilim - id_ref * id_ref));
    float iq_ref, w_sync, theta;
    float wr_ref = TWO_PI * m->f_ramp;

    if (magnetize) {
        theta = 0; iq_ref = 0; w_sync = 0; m->low_mode = 1; m->wr_est = 0;
    } else {
        int low = m->low_mode ? (fabsf(m->f_ramp) < F_SW_HI) : (fabsf(m->f_ramp) < F_SW_LO);
        if (low) {
            /* низкие частоты: угол формируется по заданию (ток задаётся, скольжение — расчётное) */
            float dir = (m->f_ramp != 0) ? sgnf(m->f_ramp) : sgnf(m->f_target);
            iq_ref = dir * 0.7f * iq_nom;
            iq_ref = clampf(iq_ref, -iq_max, iq_max);
            w_sync = wr_ref + c->rr * iq_ref / (c->lm * id_ref);
            theta = wrap_pi(m->theta + w_sync * ts);
            m->wr_est = wr_ref;
            m->int_spd = iq_ref;
        } else {
            if (m->low_mode) {
                /* безударный переход на угол наблюдателя: поворот интеграторов и токов в новую систему */
                float d = wrap_pi(th_est - m->theta), cd = cosf(d), sd = sinf(d);
                float nd = m->int_d * cd + m->int_q * sd, nq = -m->int_d * sd + m->int_q * cd;
                m->int_d = nd; m->int_q = nq;
                m->int_spd = clampf(-m->id_ref * sd + m->iq_ref * cd, -iq_max, iq_max);
                m->w_pll = m->we_f;
            }
            theta = th_est;
            w_sync = m->w_pll;
            float psi_d = psir > 0.2f * psi_nom ? psir : 0.2f * psi_nom;
            float wsl = c->rr * m->iq_f / psi_d;
            m->wr_est += 0.02f * ((w_sync - wsl) - m->wr_est);
            /* ПИ-регулятор скорости */
            float kp = c->spd_gain * iq_nom / w_rated;
            float es = wr_ref - m->wr_est;
            float out = kp * es + m->int_spd;
            iq_ref = clampf(out, -iq_max, iq_max);
            if (out == iq_ref || es * out < 0) m->int_spd += kp / (c->spd_ti > 0.01f ? c->spd_ti : 0.01f) * es * ts;
            m->int_spd = clampf(m->int_spd, -iq_max, iq_max);
            m->limiting = (out != iq_ref);
        }
        m->low_mode = (uint8_t)low;
    }
    m->we_f += 0.05f * (w_sync - m->we_f);
    m->theta = theta; m->id_ref = id_ref; m->iq_ref = iq_ref; m->psir = psir;

    /* регуляторы тока в dq с развязкой */
    float ct = cosf(theta), st = sinf(theta);
    float id = ia * ct + ib * st, iq = -ia * st + ib * ct;
    float kp = c->lsig * c->cur_bw, ki = (c->rs + c->rr) * c->cur_bw;
    float ed = id_ref - id, eq = iq_ref - iq;
    float vd = kp * ed + m->int_d - w_sync * c->lsig * iq;
    float vq = kp * eq + m->int_q + w_sync * (c->lsig * id + psi_ref);
    float vm = sqrtf(vd * vd + vq * vq);
    m->iq_f += 0.1f * (iq - m->iq_f);
    m->fw = clampf(m->fw + 10.0f * (0.93f * vmax - vm) / vmax * ts, 0.25f, 1.0f);
    if (vm > vmax) { vd *= vmax / vm; vq *= vmax / vm; }
    else { m->int_d += ki * ed * ts; m->int_q += ki * eq * ts; }
    m->int_d = clampf(m->int_d, -vmax, vmax); m->int_q = clampf(m->int_q, -vmax, vmax);
    /* компенсация запаздывания ШИМ на 1,5 периода */
    float th_o = theta + 1.5f * w_sync * ts, co = cosf(th_o), so = sinf(th_o);
    *va = vd * co - vq * so; *vb = vd * so + vq * co;

    m->f_out = m->we_f / TWO_PI;
    m->torque += 0.01f * (iq / (iq_nom > 0.1f ? iq_nom : 0.1f) * (psi_ref / psi_nom) - m->torque);
    m->speed_rpm = m->wr_est * 60.0f / (TWO_PI * (float)c->pole_pairs);
}

/* --- автонастройка: Rs (постоянный ток, две точки), Rr и Lsig (пульсирующее поле 100 Гц) --- */
static void run_tune(vfd_t *m, float *va, float *vb)
{
    const vfd_cfg_t *c = &m->c;
    float ts = c->ts;
    *vb = 0;
    m->tn_t += ts;
    if (m->tn_phase < 2) {
        float iref = (m->tn_phase == 0 ? 0.5f : 1.0f) * c->i_rated;
        *va = dc_ctrl(m, iref);
        if (m->tn_t > 1.0f) { m->tn_sv += m->va_prev; m->tn_si += m->ia; m->tn_n++; }
        if (m->tn_t > 1.5f) {
            float v = m->tn_sv / m->tn_n, i = m->tn_si / m->tn_n;
            if (m->tn_phase == 0) { m->tn_v1 = v; m->tn_i1 = i; }
            else {
                float di = i - m->tn_i1;
                if (di < 0.2f * c->i_rated) { vfd_trip(m, VFD_F_TUNE); return; }
                m->tune.rs = (v - m->tn_v1) / di;
                m->tn_amp = 0; m->tn_ph = 0; m->i_int = 0;
            }
            m->tn_phase++; m->tn_t = 0; m->tn_sv = m->tn_si = 0; m->tn_n = 0; m->tn_a = m->tn_b = 0;
        }
        return;
    }
    /* фаза 2: va = A*sin(wt); амплитуда плавно растёт до получения тока ~0,5 Iном */
    float w = TWO_PI * TUNE_F;
    int n_per = (int)(1.0f / (TUNE_F * ts) + 0.5f);          /* отсчётов на период */
    /* напряжение, действовавшее в прошедшем периоде ШИМ, отстаёт на 1,5 Ts от текущей фазы */
    float ph_v = m->tn_ph - 1.5f * w * ts;
    if (m->tn_t > 0.3f && m->tn_t <= 1.5f) {
        if (fabsf(m->ia) < 0.7f * c->i_rated) m->tn_amp += 0.5f * c->v_rated * ts;   /* набор амплитуды */
    }
    if (m->tn_t > 1.7f) {
        m->tn_a += m->ia * sinf(ph_v); m->tn_b += m->ia * cosf(ph_v); m->tn_n++;
        if (m->tn_n >= 20 * n_per) {
            float iin = 2.0f * m->tn_a / m->tn_n, iqd = 2.0f * m->tn_b / m->tn_n;
            float i2 = iin * iin + iqd * iqd;
            if (i2 < 0.01f * c->i_rated * c->i_rated) { vfd_trip(m, VFD_F_TUNE); return; }
            float r = m->tn_amp * iin / i2, x = -m->tn_amp * iqd / i2;
            m->tune.lsig = x / w;
            m->tune.rr = r - m->tune.rs;
            if (m->tune.rs <= 0 || m->tune.lsig <= 0) { vfd_trip(m, VFD_F_TUNE); return; }
            if (m->tune.rr < 0.2f * m->tune.rs) m->tune.rr = 0.2f * m->tune.rs;
            m->tune.i0 = c->i0;
            m->tune.lm = c->v_rated / (SQRT3 * TWO_PI * c->f_rated * c->i0) - m->tune.lsig;
            m->tune_done = 1; m->tune_req = 0;
            m->pwm_en = 0; enter(m, VFD_ST_STOP);
            return;
        }
    }
    *va = m->tn_amp * sinf(m->tn_ph);
    m->tn_ph = wrap_pi(m->tn_ph + w * ts);
}

void vfd_step(vfd_t *m, float iu, float iw, float vdc, float duty[3])
{
    const vfd_cfg_t *c = &m->c;
    float iv = -iu - iw;
    float va = 0, vb = 0;
    m->ia = iu; m->ib = (iv - iw) * (1.0f / SQRT3);
    m->vdc_f += 0.05f * (vdc - m->vdc_f);
    duty[0] = duty[1] = duty[2] = 0;

    /* защиты */
    if (m->state != VFD_ST_FAULT) {
        if (m->pwm_en && (fabsf(iu) > c->i_trip || fabsf(iv) > c->i_trip || fabsf(iw) > c->i_trip)) vfd_trip(m, VFD_F_OC_SW);
        else if (vdc > c->ov_trip) vfd_trip(m, VFD_F_OV);
        else if (m->pwm_en && m->vdc_f < c->uv_trip) vfd_trip(m, VFD_F_UV);
    }
    float imag = sqrtf(m->ia * m->ia + m->ib * m->ib) * (1.0f / SQRT2);
    m->i_rms += 0.01f * (imag - m->i_rms);

    m->t_state += c->ts;
    switch (m->state) {
    case VFD_ST_STOP:
        m->pwm_en = 0; m->f_out = 0; m->torque = 0; m->speed_rpm = 0; m->v_out *= 0.99f; m->power = 0; m->limiting = 0;
        if (m->run_cmd && m->vdc_f > c->uv_trip) {
            reset_ctrl(m);
            m->pwm_en = 1; enter(m, VFD_ST_BOOT);
        }
        return;
    case VFD_ST_BOOT:   /* нижние ключи открыты — заряд бутстрепных конденсаторов */
        if (m->t_state > T_BOOT) {
            if (m->tune_req) { m->tn_phase = 0; m->tn_t = 0; m->tn_sv = m->tn_si = 0; m->tn_n = 0; m->tune_done = 0; enter(m, VFD_ST_TUNE); }
            else enter(m, c->mode == VFD_MODE_SVC ? VFD_ST_MAG : VFD_ST_RUN);
        }
        return;
    case VFD_ST_MAG: {  /* намагничивание перед пуском (вектор) */
        run_svc(m, vdc, 1, &va, &vb);
        float tmag = 3.0f * c->lm / (c->rr > 0.01f ? c->rr : 0.01f);
        if (!m->run_cmd) { m->pwm_en = 0; enter(m, VFD_ST_STOP); return; }
        if (m->t_state > clampf(tmag, 0.1f, 1.0f)) enter(m, VFD_ST_RUN);
        break; }
    case VFD_ST_RUN:
        if (!m->run_cmd && (c->stop_coast || fabsf(m->f_ramp) < F_STOP)) {
            if (!c->stop_coast && c->dcb_time > 0) { m->i_int = 0; enter(m, VFD_ST_DCBRAKE); }
            else { m->pwm_en = 0; enter(m, VFD_ST_STOP); }
            return;
        }
        ramp(m, vdc);
        if (c->mode == VFD_MODE_SVC) run_svc(m, vdc, 0, &va, &vb);
        else run_vf(m, &va, &vb);
        break;
    case VFD_ST_DCBRAKE:
        va = dc_ctrl(m, c->dcb_cur); vb = 0; m->f_out = 0;
        if (m->t_state > c->dcb_time || m->run_cmd) { m->pwm_en = 0; enter(m, VFD_ST_STOP); return; }
        break;
    case VFD_ST_TUNE:
        if (!m->run_cmd) { m->pwm_en = 0; enter(m, VFD_ST_STOP); return; }
        run_tune(m, &va, &vb);
        if (m->state != VFD_ST_TUNE) return;
        break;
    case VFD_ST_FAULT:
    default:
        m->pwm_en = 0;
        return;
    }
    svpwm(m, va, vb, vdc, iu, iv, iw, duty);
    float vmag = sqrtf(m->va_prev * m->va_prev + m->vb_prev * m->vb_prev) * (SQRT3 / SQRT2);
    m->v_out += 0.01f * (vmag - m->v_out);
    m->power += 0.01f * (1.5f * (m->va_prev * m->ia + m->vb_prev * m->ib) - m->power);
}

void vfd_slow(vfd_t *m, float dt, float ol_time)
{
    /* I2t: 150 % тока в течение ol_time -> отключение; ниже 110 % — остывание */
    float r = m->i_rms / m->c.i_rated;
    m->ol_acc += (r * r - 1.21f) * dt / ((2.25f - 1.21f) * (ol_time > 1 ? ol_time : 1));
    if (m->ol_acc < 0) m->ol_acc = 0;
    if (m->ol_acc >= 1.0f && m->state != VFD_ST_FAULT) vfd_trip(m, VFD_F_OL);
}
