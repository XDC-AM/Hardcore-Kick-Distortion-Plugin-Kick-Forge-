#include "kf.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

static const char* k_char_choices[] = {"TUBE", "HARD", "FOLD", "DIODE", "GABBER", "CRUSH"};
static const char* k_os_choices[] = {"1x", "2x", "4x", "8x"};
static const char* k_clip_choices[] = {"SOFT", "HARD"};
static const char* k_offon_choices[] = {"OFF", "ON"};

const ParamDef kf_param_defs[NPARAMS] = {
    {"Input",      "IN",    "dB", -24.0f, 24.0f,   0.0f, 0, 1, 0, 1.0f, 0, 0, 1},
    {"Drive",      "DRIVE", "dB",   0.0f, 36.0f,  12.0f, 0, 0, 0, 1.0f, 0, 0, 1},
    {"Mix",        "MIX",   "%",    0.0f,100.0f, 100.0f, 0, 0, 0, 1.0f, 0, 0, 0},
    {"Character",  "CHAR",  "",     0.0f,  5.0f,   1.0f, 1, 0, 0, 1.0f, k_char_choices, 6, 0},
    {"Bias",       "BIAS",  "",    -0.5f,  0.5f,   0.0f, 0, 1, 0, 1.0f, 0, 0, 2},
    {"Low Drive",  "L.DRV", "dB",   0.0f, 36.0f,   0.0f, 0, 0, 0, 1.0f, 0, 0, 1},
    {"Mid Drive",  "M.DRV", "dB",   0.0f, 36.0f,   6.0f, 0, 0, 0, 1.0f, 0, 0, 1},
    {"High Drive", "H.DRV", "dB",   0.0f, 36.0f,  12.0f, 0, 0, 0, 1.0f, 0, 0, 1},
    {"Low Mix",    "L.MIX", "%",    0.0f,100.0f,  25.0f, 0, 0, 0, 1.0f, 0, 0, 0},
    {"Mid Mix",    "M.MIX", "%",    0.0f,100.0f, 100.0f, 0, 0, 0, 1.0f, 0, 0, 0},
    {"High Mix",   "H.MIX", "%",    0.0f,100.0f, 100.0f, 0, 0, 0, 1.0f, 0, 0, 0},
    {"Low Split",  "LO.SPL","Hz",  60.0f, 400.0f, 140.0f, 0, 0, 1, 1.0f, 0, 0, 0},
    {"High Split", "HI.SPL","Hz", 800.0f,6000.0f,2200.0f, 0, 0, 1, 1.0f, 0, 0, 0},
    {"Punch",      "PUNCH", "%",    0.0f,100.0f,  35.0f, 0, 0, 0, 1.0f, 0, 0, 0},
    {"Body",       "BODY",  "%",    0.0f,100.0f,  45.0f, 0, 0, 0, 1.0f, 0, 0, 0},
    {"Tone",       "TONE",  "%",  -100.0f,100.0f,  0.0f, 0, 1, 0, 1.0f, 0, 0, 0},
    {"Crush",      "CRUSH", "%",    0.0f,100.0f,   0.0f, 0, 0, 0, 1.0f, 0, 0, 0},
    {"Rate",       "RATE",  "x",    1.0f, 32.0f,   1.0f, 1, 0, 0, 1.0f, 0, 0, 0},
    {"Width",      "WIDTH", "%",    0.0f,200.0f, 100.0f, 0, 0, 0, 1.0f, 0, 0, 0},
    {"Mono",       "MONO",  "",     0.0f,  1.0f,   1.0f, 1, 0, 0, 1.0f, k_offon_choices, 2, 0},
    {"Low Cut",    "LO.CUT","Hz",  20.0f, 150.0f,  25.0f, 0, 0, 1, 1.0f, 0, 0, 0},
    {"Auto Gain",  "AUTO",  "",     0.0f,  1.0f,   0.0f, 1, 0, 0, 1.0f, k_offon_choices, 2, 0},
    {"Oversample", "OS",    "",     0.0f,  3.0f,   2.0f, 1, 0, 0, 1.0f, k_os_choices, 4, 0},
    {"Output",     "OUT",   "dB", -24.0f, 12.0f,  -4.0f, 0, 0, 0, 1.0f, 0, 0, 1},
    {"Ceiling",    "CEIL",  "dB", -12.0f,  0.0f,  -0.3f, 0, 0, 0, 1.0f, 0, 0, 1},
    {"Clip Mode",  "CLIP",  "",     0.0f,  1.0f,   0.0f, 1, 0, 0, 1.0f, k_clip_choices, 2, 0},
    {"Bypass",     "BYP",   "",     0.0f,  1.0f,   0.0f, 1, 0, 0, 1.0f, k_offon_choices, 2, 0}
};

float kf_param_norm_to_real(int idx, float norm) {
    const ParamDef* d;
    if (idx < 0 || idx >= NPARAMS) return 0.0f;
    d = &kf_param_defs[idx];
    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;
    if (d->discrete) {
        double span = (double)d->max - (double)d->min;
        double v = (double)d->min + floor((double)norm * span + 0.5);
        if (v < d->min) v = d->min;
        if (v > d->max) v = d->max;
        return (float)v;
    }
    if (d->logscale && d->min > 0.0f) {
        double lo = (double)d->min;
        double ratio = (double)d->max / lo;
        double t = pow(ratio, (double)norm);
        double v = lo * t;
        return (float)v;
    }
    {
        double lo = (double)d->min;
        double span = (double)d->max - lo;
        double t = pow((double)norm, (double)d->skew);
        double v = lo + span * t;
        return (float)v;
    }
}

float kf_param_real_to_norm(int idx, float real) {
    const ParamDef* d;
    double x;
    if (idx < 0 || idx >= NPARAMS) return 0.0f;
    d = &kf_param_defs[idx];
    if (real < d->min) real = d->min;
    if (real > d->max) real = d->max;
    if (d->discrete) return (float)(((double)real - (double)d->min) / ((double)d->max - (double)d->min));
    if (d->logscale && d->min > 0.0f) {
        x = log((double)real / (double)d->min) / log((double)d->max / (double)d->min);
    } else {
        x = ((double)real - (double)d->min) / ((double)d->max - (double)d->min);
        x = pow(x, 1.0 / (double)d->skew);
    }
    if (x < 0.0) x = 0.0;
    if (x > 1.0) x = 1.0;
    return (float)x;
}

float kf_param_default(int idx) {
    if (idx < 0 || idx >= NPARAMS) return 0.0f;
    return kf_param_real_to_norm(idx, kf_param_defs[idx].def);
}

void kf_param_format(int idx, float norm, char* out, int outsz) {
    const ParamDef* d;
    double v;
    int iv;
    if (!out || outsz <= 0) return;
    out[0] = 0;
    if (idx < 0 || idx >= NPARAMS) return;
    d = &kf_param_defs[idx];
    v = (double)kf_param_norm_to_real(idx, norm);
    if (d->choices) {
        iv = (int)(v + 0.5);
        if (iv < 0) iv = 0;
        if (iv >= d->nchoices) iv = d->nchoices - 1;
        sprintf(out, "%s", d->choices[iv]);
        return;
    }
    if (d->discrete) {
        sprintf(out, "%.0fx", v);
        return;
    }
    if (strcmp(d->unit, "dB") == 0) {
        if (d->bipolar && v > 0.0) sprintf(out, "+%.1f dB", v);
        else sprintf(out, "%.1f dB", v);
    } else if (strcmp(d->unit, "Hz") == 0) {
        if (v >= 1000.0) sprintf(out, "%.2f kHz", v / 1000.0);
        else sprintf(out, "%.0f Hz", v);
    } else if (strcmp(d->unit, "%") == 0) {
        if (d->bipolar && v > 0.0) sprintf(out, "+%.0f%%", v);
        else sprintf(out, "%.0f%%", v);
    } else {
        sprintf(out, "%.2f", v);
    }
}

void kf_param_label(int idx, char* out, int outsz) {
    if (!out || outsz <= 0) return;
    out[0] = 0;
    if (idx < 0 || idx >= NPARAMS) return;
    if (kf_param_defs[idx].choices) {
        sprintf(out, "%s", "");
        return;
    }
    sprintf(out, "%s", kf_param_defs[idx].unit);
}

/* input, drive, mix, char, bias, lowd, midd, highd, lowm, midm, highm,
   splitlo, splithi, punch, body, tone, crush, rate, width, mono,
   lowcut, auto, os, output, ceiling, clip, bypass */
#define PV(IN,DR,MX,CH,BI,LD,MD,HD,LM,MM,HM,SL,SH,PU,BO,TO,CR,RA,WI,MO,LC,AU,OS,OU,CE,CL,BY) \
    {IN,DR,MX,CH,BI,LD,MD,HD,LM,MM,HM,SL,SH,PU,BO,TO,CR,RA,WI,MO,LC,AU,OS,OU,CE,CL,BY}

const Preset kf_presets[KF_NUM_PRESETS] = {
    {"INIT / CLEAN", "INIT",
     PV(0,0,100,0,0.0f, 0,0,0, 100,100,100, 140,2200, 0,0,0, 0,1,100,1,20,0,2,0,-0.3f,0,0)},
    {"HARDSTYLE PUNCH", "HARD",
     PV(0,12,100,1,0.02f, 0,8,16, 25,100,100, 150,2400, 55,55,15, 0,1,100,1,28,0,2,-5,-0.3f,0,0)},
    {"RAWSTYLE SERMON", "RAW",
     PV(2,18,100,3,0.08f, 2,14,22, 20,100,100, 130,3000, 70,35,28, 5,1,120,0,30,0,2,-7,-0.3f,0,0)},
    {"FRENCHCORE FOLD", "FRENCH",
     PV(3,24,100,2,0.15f, 0,18,30, 15,100,100, 120,2600, 78,20,35, 12,3,140,0,32,0,2,-8,-0.3f,0,0)},
    {"GABBER HAMMER", "GABBER",
     PV(4,28,100,4,0.22f, 6,22,30, 35,100,100, 180,3200, 65,70,-5, 25,4,80,1,35,0,2,-9,-0.3f,1,0)},
    {"UPTEMPO STOMP", "UPTEMPO",
     PV(2,20,100,1,0.10f, 0,12,24, 30,100,85, 160,2800, 85,50,10, 8,2,110,1,30,0,2,-6,-0.3f,0,0)},
    {"INDUSTRIAL SCREAM", "INDUSTRIAL",
     PV(5,30,100,5,-0.12f, 8,26,34, 40,100,100, 220,4200, 40,25,45, 55,8,160,0,45,0,1,-10,-0.3f,1,0)},
    {"SUB KEEPER", "SUB",
     PV(0,8,70,0,0.0f, 0,4,2, 60,45,20, 110,1800, 25,85,-25, 0,1,60,1,20,0,2,-3,-0.3f,0,0)},
    {"TAIL BURNER", "TAIL",
     PV(1,16,100,0,0.04f, 2,10,18, 35,90,100, 140,2400, 10,100,20, 0,1,130,0,25,0,2,-5,-0.3f,0,0)},
    {"TOTAL DESTRUCTION", "TOTAL",
     PV(8,36,100,4,0.30f, 12,30,36, 50,100,100, 250,5000, 100,80,50, 70,16,200,0,60,0,1,-14,-0.5f,1,0)}
};

const char* kf_preset_name(int idx) {
    if (idx < 0 || idx >= KF_NUM_PRESETS) return "INIT / CLEAN";
    return kf_presets[idx].name;
}

void kf_snapshot(const float* params, ParamSnapshot* s) {
    int i;
    if (!params || !s) return;
    s->input_gain = kf_db2lin(kf_param_norm_to_real(P_INPUT, params[P_INPUT]));
    s->drive_gain = kf_db2lin(kf_param_norm_to_real(P_DRIVE, params[P_DRIVE]));
    for (i = 0; i < 3; i++) {
        int di = P_LOW_DRIVE + i;
        int mi = P_LOW_MIX + i;
        s->band_gain[i] = kf_db2lin(kf_param_norm_to_real(P_DRIVE, params[P_DRIVE]) +
                                    kf_param_norm_to_real(di, params[di]));
        s->band_mix[i] = (double)kf_param_norm_to_real(mi, params[mi]) * 0.01;
    }
    s->mix = (double)kf_param_norm_to_real(P_MIX, params[P_MIX]) * 0.01;
    s->char_mode = (int)(kf_param_norm_to_real(P_CHAR, params[P_CHAR]) + 0.5);
    s->bias = (double)kf_param_norm_to_real(P_BIAS, params[P_BIAS]);
    s->split_low = (double)kf_param_norm_to_real(P_SPLIT_LOW, params[P_SPLIT_LOW]);
    s->split_high = (double)kf_param_norm_to_real(P_SPLIT_HIGH, params[P_SPLIT_HIGH]);
    s->punch = (double)kf_param_norm_to_real(P_PUNCH, params[P_PUNCH]) * 0.01;
    s->body_db = (double)kf_param_norm_to_real(P_BODY, params[P_BODY]) * 0.12;
    s->tone = (double)kf_param_norm_to_real(P_TONE, params[P_TONE]) * 0.01;
    s->crush = (double)kf_param_norm_to_real(P_CRUSH, params[P_CRUSH]) * 0.01;
    s->rate = (double)kf_param_norm_to_real(P_RATE, params[P_RATE]);
    s->width = (double)kf_param_norm_to_real(P_WIDTH, params[P_WIDTH]) * 0.01;
    s->mono = (kf_param_norm_to_real(P_MONO, params[P_MONO]) >= 0.5) ? 1 : 0;
    s->lowcut = (double)kf_param_norm_to_real(P_LOWCUT, params[P_LOWCUT]);
    s->auto_gain = (kf_param_norm_to_real(P_AUTO, params[P_AUTO]) >= 0.5) ? 1 : 0;
    s->os = (int)(kf_param_norm_to_real(P_OS, params[P_OS]) + 0.5);
    s->output_gain = kf_db2lin(kf_param_norm_to_real(P_OUTPUT, params[P_OUTPUT]));
    s->ceiling = kf_db2lin(kf_param_norm_to_real(P_CEILING, params[P_CEILING]));
    s->clip_mode = (kf_param_norm_to_real(P_CLIP, params[P_CLIP]) >= 0.5) ? 1 : 0;
    s->bypass = (kf_param_norm_to_real(P_BYPASS, params[P_BYPASS]) >= 0.5) ? 1 : 0;
    if (s->ceiling < 0.05) s->ceiling = 0.05;
    if (s->ceiling > 1.0) s->ceiling = 1.0;
    if (s->char_mode < 0) s->char_mode = 0;
    if (s->char_mode > 5) s->char_mode = 5;
    if (s->os < 0) s->os = 0;
    if (s->os > 3) s->os = 3;
}
