#include "kf.h"
#include <math.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Halfband FIR resampler                                              */
/* ------------------------------------------------------------------ */
static double g_hb[KF_HB_TAPS];
static int g_hb_ready = 0;

static void hb_init(void) {
    int n;
    double sum = 0.0;
    if (g_hb_ready) return;
    for (n = 0; n < KF_HB_TAPS; n++) {
        double x = (double)(n - (KF_HB_TAPS - 1) / 2);
        double s;
        double w;
        if (fabs(x) < 1e-12) s = 1.0;
        else s = sin(KF_PI * 0.5 * x) / (KF_PI * 0.5 * x);
        w = 0.42 - 0.5 * cos(2.0 * KF_PI * n / (double)(KF_HB_TAPS - 1)) +
            0.08 * cos(4.0 * KF_PI * n / (double)(KF_HB_TAPS - 1));
        g_hb[n] = 0.5 * s * w;
        sum += g_hb[n];
    }
    if (fabs(sum) > 1e-12) {
        for (n = 0; n < KF_HB_TAPS; n++) g_hb[n] /= sum;
    }
    g_hb_ready = 1;
}

static double hb_push(HbState* st, double x) {
    int k;
    double y = 0.0;
    for (k = KF_HB_TAPS - 1; k > 0; k--) st->buf[k] = st->buf[k - 1];
    st->buf[0] = x;
    for (k = 0; k < KF_HB_TAPS; k++) y += g_hb[k] * st->buf[k];
    return y;
}

static void upsample2(HbState* st, const double* in, int n, double* out) {
    int i;
    for (i = 0; i < n; i++) {
        out[2 * i] = hb_push(st, 2.0 * in[i]);
        out[2 * i + 1] = hb_push(st, 0.0);
    }
}

static void downsample2(HbState* st, const double* in, int n, double* out) {
    int i;
    for (i = 0; i < n; i++) {
        double y = hb_push(st, in[i]);
        if (i & 1) out[i >> 1] = y;
    }
}

/* ------------------------------------------------------------------ */
/* State variable filters                                              */
/* ------------------------------------------------------------------ */
static SvfCoef svf_make(double freq, double q, double sr) {
    SvfCoef c;
    double g = tan(KF_PI * freq / sr);
    if (g < 1.0e-9) g = 1.0e-9;
    c.k = 1.0 / q;
    c.a1 = 1.0 / (1.0 + g * (g + c.k));
    c.a2 = g * c.a1;
    c.a3 = g * c.a2;
    return c;
}

static double svf_lp(SvfState* s, const SvfCoef* c, double x) {
    double v3 = x - s->ic2;
    double v1 = c->a1 * s->ic1 + c->a2 * v3;
    double v2 = s->ic2 + c->a2 * s->ic1 + c->a3 * v3;
    s->ic1 = 2.0 * v1 - s->ic1;
    s->ic2 = 2.0 * v2 - s->ic2;
    if (fabs(s->ic1) < 1.0e-20) s->ic1 = 0.0;
    if (fabs(s->ic2) < 1.0e-20) s->ic2 = 0.0;
    return v2;
}

static double svf_hp(SvfState* s, const SvfCoef* c, double x) {
    double v3 = x - s->ic2;
    double v1 = c->a1 * s->ic1 + c->a2 * v3;
    double v2 = s->ic2 + c->a2 * s->ic1 + c->a3 * v3;
    s->ic1 = 2.0 * v1 - s->ic1;
    s->ic2 = 2.0 * v2 - s->ic2;
    if (fabs(s->ic1) < 1.0e-20) s->ic1 = 0.0;
    if (fabs(s->ic2) < 1.0e-20) s->ic2 = 0.0;
    return x - c->k * v1 - v2;
}

static double svf_ap(SvfState* s, const SvfCoef* c, double x) {
    double v3 = x - s->ic2;
    double v1 = c->a1 * s->ic1 + c->a2 * v3;
    double v2 = s->ic2 + c->a2 * s->ic1 + c->a3 * v3;
    s->ic1 = 2.0 * v1 - s->ic1;
    s->ic2 = 2.0 * v2 - s->ic2;
    if (fabs(s->ic1) < 1.0e-20) s->ic1 = 0.0;
    if (fabs(s->ic2) < 1.0e-20) s->ic2 = 0.0;
    return x - 2.0 * c->k * v1;
}

/* ------------------------------------------------------------------ */
/* Biquads (RBJ cookbook)                                              */
/* ------------------------------------------------------------------ */
static void biquad_identity(Biquad* b) {
    b->b0 = 1.0; b->b1 = 0.0; b->b2 = 0.0;
    b->a1 = 0.0; b->a2 = 0.0;
}

static double biquad_process(Biquad* b, double x) {
    double y = b->b0 * x + b->z1;
    b->z1 = b->b1 * x - b->a1 * y + b->z2;
    b->z2 = b->b2 * x - b->a2 * y;
    if (fabs(b->z1) < 1.0e-20) b->z1 = 0.0;
    if (fabs(b->z2) < 1.0e-20) b->z2 = 0.0;
    return y;
}

static void biquad_lowshelf(Biquad* b, double f, double gain_db, double sr) {
    double A, w0, cw, sw, alpha, beta, a0;
    if (fabs(gain_db) < 0.01 || f < 5.0 || f > sr * 0.45) {
        biquad_identity(b);
        return;
    }
    A = pow(10.0, gain_db / 40.0);
    w0 = 2.0 * KF_PI * f / sr;
    cw = cos(w0); sw = sin(w0);
    alpha = sw * 0.5 * sqrt((A + 1.0 / A) * (1.0 / 1.0 - 1.0) + 2.0);
    beta = 2.0 * sqrt(A) * alpha;
    a0 = (A + 1.0) + (A - 1.0) * cw + beta;
    b->b0 = A * ((A + 1.0) - (A - 1.0) * cw + beta) / a0;
    b->b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cw) / a0;
    b->b2 = A * ((A + 1.0) - (A - 1.0) * cw - beta) / a0;
    b->a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cw) / a0;
    b->a2 = ((A + 1.0) + (A - 1.0) * cw - beta) / a0;
}

static void biquad_highshelf(Biquad* b, double f, double gain_db, double sr) {
    double A, w0, cw, sw, alpha, beta, a0;
    if (fabs(gain_db) < 0.01 || f < 5.0 || f > sr * 0.45) {
        biquad_identity(b);
        return;
    }
    A = pow(10.0, gain_db / 40.0);
    w0 = 2.0 * KF_PI * f / sr;
    cw = cos(w0); sw = sin(w0);
    alpha = sw * 0.5 * sqrt((A + 1.0 / A) * (1.0 / 1.0 - 1.0) + 2.0);
    beta = 2.0 * sqrt(A) * alpha;
    a0 = (A + 1.0) - (A - 1.0) * cw + beta;
    b->b0 = A * ((A + 1.0) + (A - 1.0) * cw + beta) / a0;
    b->b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cw) / a0;
    b->b2 = A * ((A + 1.0) + (A - 1.0) * cw - beta) / a0;
    b->a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cw) / a0;
    b->a2 = ((A + 1.0) - (A - 1.0) * cw - beta) / a0;
}

static void biquad_highpass(Biquad* b, double f, double q, double sr) {
    double w0, cw, sw, alpha, a0;
    if (f < 5.0 || f > sr * 0.45) {
        biquad_identity(b);
        return;
    }
    w0 = 2.0 * KF_PI * f / sr;
    cw = cos(w0); sw = sin(w0);
    alpha = sw / (2.0 * q);
    a0 = 1.0 + alpha;
    b->b0 = (1.0 + cw) * 0.5 / a0;
    b->b1 = -(1.0 + cw) / a0;
    b->b2 = (1.0 + cw) * 0.5 / a0;
    b->a1 = -2.0 * cw / a0;
    b->a2 = (1.0 - alpha) / a0;
}

/* ------------------------------------------------------------------ */
/* Shapers                                                             */
/* ------------------------------------------------------------------ */
static double kf_fold(double x) {
    return sin(x * 1.5707963267948966);
}

static double kf_shape(double x, int mode, double bias) {
    double y, a, s;
    x += bias;
    switch (mode) {
    case 0: /* TUBE */
        y = tanh(x * (1.0 + 0.12 * x * x));
        return y;
    case 1: /* HARD */
        a = fabs(x);
        s = (x < 0.0) ? -1.0 : 1.0;
        if (a > 0.90) a = 0.90 + (1.0 - exp(-(a - 0.90) * 7.0)) * 0.10;
        return s * a;
    case 2: /* FOLD */
        return kf_fold(x);
    case 3: /* DIODE */
        if (x >= 0.0) return 1.0 - exp(-1.55 * x);
        return -0.86 * (1.0 - exp(1.15 * x));
    case 4: /* GABBER */
        y = kf_clamp(x * 1.45, -1.2, 1.2);
        y = kf_fold(y);
        y = kf_clamp(y * 2.35, -1.0, 1.0);
        return y;
    case 5: /* CRUSH */
        y = kf_clamp(x, -1.0, 1.0);
        y = floor(y * 8.0 + 0.5) * 0.125;
        return y;
    default:
        return tanh(x);
    }
}

static double xover_comp(double g) {
    double c;
    if (g < 1.0) g = 1.0;
    c = 1.0 / (1.0 + 0.18 * (g - 1.0));
    if (c < 0.06) c = 0.06;
    return c;
}

static const double g_mode_comp[6] = {1.00, 0.95, 0.70, 0.72, 0.45, 0.90};

/* ------------------------------------------------------------------ */
/* Init / reset                                                        */
/* ------------------------------------------------------------------ */
void kf_dsp_init(Dsp* d, float sr) {
    memset(d, 0, sizeof(*d));
    hb_init();
    d->sr = sr > 0.0f ? sr : 44100.0f;
    d->cur_os = -1;
    d->coef_valid = 0;
    d->char_mode = 1;
    d->s_input_gain = 1.0;
    d->s_drive_gain = 1.0;
    d->s_band_gain[0] = d->s_band_gain[1] = d->s_band_gain[2] = 1.0;
    d->s_band_mix[0] = d->s_band_mix[1] = d->s_band_mix[2] = 1.0;
    d->s_mix = 1.0;
    d->s_width = 1.0;
    d->s_split_low = 140.0;
    d->s_split_high = 2200.0;
    d->s_lowcut = 25.0;
    d->s_output_gain = 1.0;
    d->s_ceiling = 0.98;
    d->s_auto_gain = 1.0;
    d->os = 2;
}

void kf_dsp_prime(Dsp* d, const ParamSnapshot* p) {
    int i;
    if (!d || !p) return;
    d->s_input_gain = p->input_gain;
    d->s_drive_gain = p->drive_gain;
    for (i = 0; i < 3; i++) {
        d->s_band_gain[i] = p->band_gain[i];
        d->s_band_mix[i] = p->band_mix[i];
    }
    d->s_mix = p->mix;
    d->s_bias = p->bias;
    d->s_punch = p->punch;
    d->s_body_db = p->body_db;
    d->s_tone = p->tone;
    d->s_crush = p->crush;
    d->s_rate = p->rate;
    d->s_width = p->width;
    d->s_split_low = p->split_low;
    d->s_split_high = p->split_high;
    d->s_lowcut = p->lowcut;
    d->s_output_gain = p->output_gain;
    d->s_ceiling = p->ceiling;
    d->s_auto_gain = 1.0;
    d->char_mode = p->char_mode;
    d->mono = p->mono;
    d->auto_gain = p->auto_gain;
    d->os = p->os;
    d->clip_mode = p->clip_mode;
    d->bypass = p->bypass;
    d->cur_os = p->os;
    d->coef_valid = 0;
}

void kf_dsp_reset(Dsp* d) {
    int c;
    for (c = 0; c < 2; c++) {
        memset(&d->ch[c], 0, sizeof(Channel));
        biquad_identity(&d->ch[c].body);
        biquad_identity(&d->ch[c].tilt_lo);
        biquad_identity(&d->ch[c].tilt_hi);
        biquad_identity(&d->ch[c].lowcut);
    }
    d->cur_os = -1;
    d->coef_valid = 0;
    d->s_auto_gain = 1.0;
}

void kf_dsp_set_sr(Dsp* d, float sr) {
    if (sr < 8000.0f) sr = 8000.0f;
    if (sr > 384000.0f) sr = 384000.0f;
    d->sr = sr;
    kf_dsp_reset(d);
}

/* ------------------------------------------------------------------ */
/* Main process                                                        */
/* ------------------------------------------------------------------ */
void kf_dsp_process(Dsp* d, const float* inL, const float* inR,
                    float* outL, float* outR, int n, const ParamSnapshot* p) {
    int off;
    int c;
    double sr;

    if (!d || !p || n <= 0) return;
    sr = (double)d->sr;
    if (n > KF_MAX_BLOCK) n = KF_MAX_BLOCK;

    if (d->cur_os != p->os) {
        for (c = 0; c < 2; c++) {
            memset(d->ch[c].up, 0, sizeof(d->ch[c].up));
            memset(d->ch[c].down, 0, sizeof(d->ch[c].down));
            memset(&d->ch[c].xo, 0, sizeof(d->ch[c].xo));
            memset(d->ch[c].dry, 0, sizeof(d->ch[c].dry));
            d->ch[c].dry_pos = 0;
            d->ch[c].crush_count = 0;
            d->ch[c].punch_fast = 0.0;
            d->ch[c].punch_slow = 0.0;
            d->ch[c].auto_rms = 0.0;
        }
        d->cur_os = p->os;
        d->coef_valid = 0;
    }

    for (off = 0; off < n; off += 64) {
        int m = n - off;
        int i;
        int stages;
        int wn;
        double sm;
        double fs;
        double *work;
        double bg[3], bmix[3], comp[3];
        double a_att, a_rel, b_att, b_rel;
        double levels, inv_levels;
        double ag_target, ag_coef;
        double rms_acc = 0.0;
        int delay;

        if (m > 64) m = 64;
        sm = 1.0 - exp(-(double)m / (sr * 0.018));
        fs = sr * (double)(1 << p->os);
        stages = p->os;
        delay = (stages == 0) ? 0 : (stages == 1 ? 8 : (stages == 2 ? 11 : 13));

        /* parameter smoothing */
        d->s_input_gain += (p->input_gain - d->s_input_gain) * sm;
        d->s_drive_gain += (p->drive_gain - d->s_drive_gain) * sm;
        for (i = 0; i < 3; i++) {
            d->s_band_gain[i] += (p->band_gain[i] - d->s_band_gain[i]) * sm;
            d->s_band_mix[i] += (p->band_mix[i] - d->s_band_mix[i]) * sm;
        }
        d->s_mix += (p->mix - d->s_mix) * sm;
        d->s_bias += (p->bias - d->s_bias) * sm;
        d->s_punch += (p->punch - d->s_punch) * sm;
        d->s_body_db += (p->body_db - d->s_body_db) * sm;
        d->s_tone += (p->tone - d->s_tone) * sm;
        d->s_crush += (p->crush - d->s_crush) * sm;
        d->s_rate += (p->rate - d->s_rate) * sm;
        d->s_width += (p->width - d->s_width) * sm;
        d->s_split_low += (p->split_low - d->s_split_low) * sm;
        d->s_split_high += (p->split_high - d->s_split_high) * sm;
        d->s_lowcut += (p->lowcut - d->s_lowcut) * sm;
        d->s_output_gain += (p->output_gain - d->s_output_gain) * sm;
        d->s_ceiling += (p->ceiling - d->s_ceiling) * sm;
        d->char_mode = p->char_mode;
        d->mono = p->mono;
        d->auto_gain = p->auto_gain;
        d->clip_mode = p->clip_mode;
        d->bypass = p->bypass;

        if (d->s_split_high < d->s_split_low * 1.25) d->s_split_high = d->s_split_low * 1.25;

        /* crossover coefficients at oversampled rate */
        d->xc.lp1 = svf_make(d->s_split_low, 0.70710678, fs);
        d->xc.hp1 = d->xc.lp1;
        d->xc.lp2 = svf_make(d->s_split_high, 0.70710678, fs);
        d->xc.hp2 = d->xc.lp2;
        d->xc.ap1 = d->xc.lp1;
        d->xc.ap2 = d->xc.lp2;
        d->coef_valid = 1;

        /* base-rate EQ coefficients */
        for (c = 0; c < 2; c++) {
            biquad_lowshelf(&d->ch[c].body, 82.0, d->s_body_db, sr);
            biquad_lowshelf(&d->ch[c].tilt_lo, 120.0, -d->s_tone * 6.0, sr);
            biquad_highshelf(&d->ch[c].tilt_hi, 3500.0, d->s_tone * 6.0, sr);
            biquad_highpass(&d->ch[c].lowcut, d->s_lowcut, 0.70710678, sr);
        }

        bg[0] = d->s_band_gain[0];
        bg[1] = d->s_band_gain[1];
        bg[2] = d->s_band_gain[2];
        bmix[0] = d->s_band_mix[0];
        bmix[1] = d->s_band_mix[1];
        bmix[2] = d->s_band_mix[2];
        comp[0] = xover_comp(bg[0]);
        comp[1] = xover_comp(bg[1]);
        comp[2] = xover_comp(bg[2]);
        {
            int ci = d->char_mode;
            double mc;
            if (ci < 0) ci = 0;
            if (ci > 5) ci = 5;
            mc = g_mode_comp[ci];
            comp[0] *= mc;
            comp[1] *= mc;
            comp[2] *= mc;
        }

        /* per-channel band processing */
        for (c = 0; c < 2; c++) {
            Channel* ch = &d->ch[c];
            const float* src = (c == 0) ? inL : inR;
            if (!src) {
                for (i = 0; i < m; i++) ch->dry_in[i] = 0.0;
            } else {
                for (i = 0; i < m; i++) {
                    double x = (double)src[off + i] * d->s_input_gain;
                    double y = x - ch->dc_x1 + 0.9995 * ch->dc_y1;
                    ch->dc_x1 = x;
                    ch->dc_y1 = y;
                    if (fabs(ch->dc_y1) < 1.0e-20) ch->dc_y1 = 0.0;
                    ch->dry_in[i] = y;
                    ch->bufA[i] = y;
                }
            }

            work = ch->bufA;
            wn = m;
            if (stages >= 1) {
                upsample2(&ch->up[0], ch->bufA, m, ch->bufB);
                work = ch->bufB;
                wn = m * 2;
            }
            if (stages >= 2) {
                upsample2(&ch->up[1], work, wn, ch->bufA);
                work = ch->bufA;
                wn *= 2;
            }
            if (stages >= 3) {
                upsample2(&ch->up[2], work, wn, ch->bufB);
                work = ch->bufB;
                wn *= 2;
            }

            for (i = 0; i < wn; i++) {
                double x = work[i];
                double low, mid, high, mh;
                double yl, ym, yh;

                low = svf_lp(&ch->xo.lp1a, &d->xc.lp1, x);
                low = svf_lp(&ch->xo.lp1b, &d->xc.lp1, low);

                mh = svf_hp(&ch->xo.hp1a, &d->xc.hp1, x);
                mh = svf_hp(&ch->xo.hp1b, &d->xc.hp1, mh);
                mid = svf_lp(&ch->xo.lp2a, &d->xc.lp2, mh);
                mid = svf_lp(&ch->xo.lp2b, &d->xc.lp2, mid);

                high = svf_hp(&ch->xo.hp2a, &d->xc.hp2, x);
                high = svf_hp(&ch->xo.hp2b, &d->xc.hp2, high);

                low = svf_ap(&ch->xo.ap2a, &d->xc.ap2, low);
                low = svf_ap(&ch->xo.ap2b, &d->xc.ap2, low);
                high = svf_ap(&ch->xo.ap1a, &d->xc.ap1, high);
                high = svf_ap(&ch->xo.ap1b, &d->xc.ap1, high);

                yl = kf_shape(low * bg[0], d->char_mode, d->s_bias) * comp[0];
                ym = kf_shape(mid * bg[1], d->char_mode, d->s_bias) * comp[1];
                yh = kf_shape(high * bg[2], d->char_mode, d->s_bias) * comp[2];

                low = low * (1.0 - bmix[0]) + yl * bmix[0];
                mid = mid * (1.0 - bmix[1]) + ym * bmix[1];
                high = high * (1.0 - bmix[2]) + yh * bmix[2];

                work[i] = low + mid + high;
            }

            if (stages >= 3) {
                downsample2(&ch->down[2], work, wn, ch->bufA);
                work = ch->bufA;
                wn >>= 1;
            }
            if (stages >= 2) {
                downsample2(&ch->down[1], work, wn, ch->bufB);
                work = ch->bufB;
                wn >>= 1;
            }
            if (stages >= 1) {
                downsample2(&ch->down[0], work, wn, ch->bufA);
                work = ch->bufA;
                wn >>= 1;
            }
            /* now ch->bufA holds m base-rate wet samples */
        }

        /* shared envelope / post processing */
        a_att = exp(-1.0 / (sr * 0.0005));
        a_rel = exp(-1.0 / (sr * 0.020));
        b_att = exp(-1.0 / (sr * 0.008));
        b_rel = exp(-1.0 / (sr * 0.180));

        levels = pow(2.0, 16.0 - d->s_crush * 14.0);
        if (levels < 4.0) levels = 4.0;
        inv_levels = 1.0 / levels;

        ag_target = 1.0;
        if (d->auto_gain) {
            double r = d->ch[0].auto_rms;
            if (r > 1.0e-8) {
                ag_target = 0.25 / sqrt(r);
                if (ag_target < 0.25) ag_target = 0.25;
                if (ag_target > 4.0) ag_target = 4.0;
            }
        }
        ag_coef = 1.0 - exp(-(double)m / (sr * 0.060));
        d->s_auto_gain += (ag_target - d->s_auto_gain) * ag_coef;

        for (i = 0; i < m; i++) {
            Channel* c0 = &d->ch[0];
            Channel* c1 = &d->ch[1];
            double l = c0->bufA[i] * d->s_auto_gain;
            double r = c1->bufA[i] * d->s_auto_gain;
            double mono, am, fast, slow, tr, pg;
            double dryl, dryr;
            int ri;

            /* output DC block */
            {
                double y = l - c0->odc_x1 + 0.9995 * c0->odc_y1;
                c0->odc_x1 = l; c0->odc_y1 = y; l = y;
            }
            {
                double y = r - c1->odc_x1 + 0.9995 * c1->odc_y1;
                c1->odc_x1 = r; c1->odc_y1 = y; r = y;
            }

            /* transient punch, linked stereo */
            mono = 0.5 * (l + r);
            am = fabs(mono);
            fast = c0->punch_fast;
            slow = c0->punch_slow;
            fast = (am > fast) ? (a_att * fast + (1.0 - a_att) * am)
                               : (a_rel * fast + (1.0 - a_rel) * am);
            slow = (am > slow) ? (b_att * slow + (1.0 - b_att) * am)
                               : (b_rel * slow + (1.0 - b_rel) * am);
            c0->punch_fast = fast;
            c0->punch_slow = slow;
            tr = fast - slow;
            if (tr < 0.0) tr = 0.0;
            pg = 1.0 + d->s_punch * 4.0 * tr;
            pg = kf_clamp(pg, 1.0, 4.5);
            l *= pg;
            r *= pg;

            /* body + tilt */
            l = biquad_process(&c0->body, l);
            r = biquad_process(&c1->body, r);
            l = biquad_process(&c0->tilt_lo, l);
            r = biquad_process(&c1->tilt_lo, r);
            l = biquad_process(&c0->tilt_hi, l);
            r = biquad_process(&c1->tilt_hi, r);

            /* bit / sample-rate crush */
            if (c0->crush_count <= 0) {
                c0->crush_hold = l;
                c0->crush_count = (int)(d->s_rate + 0.5);
                if (c0->crush_count < 1) c0->crush_count = 1;
            }
            if (c1->crush_count <= 0) {
                c1->crush_hold = r;
                c1->crush_count = (int)(d->s_rate + 0.5);
                if (c1->crush_count < 1) c1->crush_count = 1;
            }
            c0->crush_count--;
            c1->crush_count--;
            l = floor(c0->crush_hold * levels + 0.5) * inv_levels;
            r = floor(c1->crush_hold * levels + 0.5) * inv_levels;

            /* width / mono */
            if (d->mono) {
                mono = 0.5 * (l + r);
                l = mono;
                r = mono;
            } else {
                double mid = 0.5 * (l + r);
                double side = 0.5 * (l - r) * d->s_width;
                l = mid + side;
                r = mid - side;
            }

            /* low cut */
            l = biquad_process(&c0->lowcut, l);
            r = biquad_process(&c1->lowcut, r);

            /* wet / dry */
            c0->dry[c0->dry_pos & 63] = c0->dry_in[i];
            c1->dry[c1->dry_pos & 63] = c1->dry_in[i];
            ri = (c0->dry_pos - delay) & 63;
            dryl = c0->dry[ri];
            ri = (c1->dry_pos - delay) & 63;
            dryr = c1->dry[ri];
            c0->dry_pos = (c0->dry_pos + 1) & 63;
            c1->dry_pos = (c1->dry_pos + 1) & 63;
            l = dryl * (1.0 - d->s_mix) + l * d->s_mix;
            r = dryr * (1.0 - d->s_mix) + r * d->s_mix;

            /* output / ceiling */
            l *= d->s_output_gain;
            r *= d->s_output_gain;
            if (d->clip_mode == 1) {
                l = kf_clamp(l, -d->s_ceiling, d->s_ceiling);
                r = kf_clamp(r, -d->s_ceiling, d->s_ceiling);
            } else {
                l = d->s_ceiling * tanh(l / d->s_ceiling);
                r = d->s_ceiling * tanh(r / d->s_ceiling);
            }
            if (l < -4.0) l = -4.0;
            if (l > 4.0) l = 4.0;
            if (r < -4.0) r = -4.0;
            if (r > 4.0) r = 4.0;

            rms_acc += l * l + r * r;
            outL[off + i] = (float)l;
            outR[off + i] = (float)r;
        }

        d->ch[0].auto_rms = rms_acc / (double)(2 * m);
        d->ch[1].auto_rms = d->ch[0].auto_rms;
    }
}
