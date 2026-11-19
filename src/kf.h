/* KickForge - shared engine / plugin definitions */
#ifndef KF_H
#define KF_H

#include <stdint.h>
#include <windows.h>
#include <math.h>
#include "vst2.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KF_VERSION 2102
#define KF_UI_W 1080
#define KF_UI_H 700
#define KF_MAX_BLOCK 4096
#define KF_MAX_OS 8
#define KF_SCOPE_SIZE 2048
#define KF_HB_TAPS 31
#define KF_PI 3.14159265358979323846

/* ------------------------------------------------------------------ */
/* Parameters                                                          */
/* ------------------------------------------------------------------ */
enum {
    P_INPUT = 0,
    P_DRIVE,
    P_MIX,
    P_CHAR,
    P_BIAS,
    P_LOW_DRIVE,
    P_MID_DRIVE,
    P_HIGH_DRIVE,
    P_LOW_MIX,
    P_MID_MIX,
    P_HIGH_MIX,
    P_SPLIT_LOW,
    P_SPLIT_HIGH,
    P_PUNCH,
    P_BODY,
    P_TONE,
    P_CRUSH,
    P_RATE,
    P_WIDTH,
    P_MONO,
    P_LOWCUT,
    P_AUTO,
    P_OS,
    P_OUTPUT,
    P_CEILING,
    P_CLIP,
    P_BYPASS,
    NPARAMS
};

typedef struct ParamDef {
    const char* name;
    const char* shortname;
    const char* unit;
    float min;
    float max;
    float def;
    int discrete;
    int bipolar;
    int logscale;
    float skew;
    const char* const* choices;
    int nchoices;
    int decimals;
} ParamDef;

extern const ParamDef kf_param_defs[NPARAMS];

float kf_param_norm_to_real(int idx, float norm);
float kf_param_real_to_norm(int idx, float real);
float kf_param_default(int idx);
void kf_param_format(int idx, float norm, char* out, int outsz);
void kf_param_label(int idx, char* out, int outsz);

typedef struct Preset {
    const char* name;
    const char* tag;
    float values[NPARAMS];
} Preset;

#define KF_NUM_PRESETS 10
extern const Preset kf_presets[KF_NUM_PRESETS];
const char* kf_preset_name(int idx);

/* ------------------------------------------------------------------ */
/* DSP                                                                 */
/* ------------------------------------------------------------------ */
typedef struct ParamSnapshot {
    double input_gain;
    double drive_gain;
    double band_gain[3];
    double band_mix[3];
    double mix;
    int char_mode;
    double bias;
    double split_low;
    double split_high;
    double punch;
    double body_db;
    double tone;
    double crush;
    double rate;
    double width;
    int mono;
    int auto_gain;
    int os;
    int clip_mode;
    int bypass;
    double lowcut;
    double output_gain;
    double ceiling;
} ParamSnapshot;

void kf_snapshot(const float* params, ParamSnapshot* s);

typedef struct HbState {
    double buf[KF_HB_TAPS];
} HbState;

typedef struct SvfCoef {
    double a1, a2, a3, k;
} SvfCoef;

typedef struct SvfState {
    double ic1, ic2;
} SvfState;

typedef struct XoverCoef {
    SvfCoef lp1, hp1, lp2, hp2, ap1, ap2;
} XoverCoef;

typedef struct XoverState {
    SvfState lp1a, lp1b;
    SvfState hp1a, hp1b;
    SvfState lp2a, lp2b;
    SvfState hp2a, hp2b;
    SvfState ap1a, ap1b;
    SvfState ap2a, ap2b;
} XoverState;

typedef struct Biquad {
    double b0, b1, b2, a1, a2;
    double z1, z2;
} Biquad;

typedef struct Channel {
    double dc_x1, dc_y1;
    double odc_x1, odc_y1;
    XoverState xo;
    Biquad body, tilt_lo, tilt_hi, lowcut;
    HbState up[3];
    HbState down[3];
    double dry[64];
    int dry_pos;
    double dry_in[KF_MAX_BLOCK];
    double punch_fast, punch_slow;
    double crush_hold;
    int crush_count;
    double auto_rms;
    double bufA[KF_MAX_BLOCK * KF_MAX_OS];
    double bufB[KF_MAX_BLOCK * KF_MAX_OS];
} Channel;

typedef struct Dsp {
    float sr;
    int cur_os;
    int coef_valid;
    XoverCoef xc;

    /* smoothed values */
    double s_input_gain;
    double s_drive_gain;
    double s_band_gain[3];
    double s_band_mix[3];
    double s_mix;
    double s_bias;
    double s_punch;
    double s_body_db;
    double s_tone;
    double s_crush;
    double s_rate;
    double s_width;
    double s_split_low;
    double s_split_high;
    double s_lowcut;
    double s_output_gain;
    double s_ceiling;
    double s_auto_gain;
    int char_mode;
    int mono;
    int auto_gain;
    int os;
    int clip_mode;
    int bypass;

    Channel ch[2];
} Dsp;

void kf_dsp_init(Dsp* d, float sr);
void kf_dsp_reset(Dsp* d);
void kf_dsp_prime(Dsp* d, const ParamSnapshot* p);
void kf_dsp_set_sr(Dsp* d, float sr);
void kf_dsp_process(Dsp* d, const float* inL, const float* inR,
                    float* outL, float* outR, int n, const ParamSnapshot* p);

/* ------------------------------------------------------------------ */
/* Plugin                                                              */
/* ------------------------------------------------------------------ */
typedef struct Plugin {
    AEffect effect;
    audioMasterCallback host;
    float sr;
    int block_size;
    int suspended;
    int host_bypass;
    int current_preset;
    int preset_dirty;
    float params[NPARAMS];
    ParamSnapshot snap;
    Dsp dsp;
    void* editor;
    ERect edit_rect;
    int ui_scale;   /* 0=100%, 1=125%, 2=150% */
    int ui_theme;   /* 0=original, 1=magenta, 2=crimson, 3=ink blue */
    int ui_style;   /* 0=Darkaero Chrome, 1=Darkaero Brood */
    int ui_bg;      /* legacy; no image background */

    unsigned char chunk[4096];
    int chunk_size;

    volatile float peak_l;
    volatile float peak_r;
    volatile float rms_l;
    volatile float rms_r;
    volatile float clip_led;
    volatile float scope[KF_SCOPE_SIZE];
    volatile int scope_pos;
    unsigned int rng;
} Plugin;

void kf_plugin_init(Plugin* p, audioMasterCallback host);
void kf_plugin_destroy(Plugin* p);
void kf_plugin_set_sr(Plugin* p, float sr);
void kf_plugin_set_block(Plugin* p, int n);
void kf_plugin_reset(Plugin* p);
void kf_plugin_process(Plugin* p, float** inputs, float** outputs, int n);
void kf_plugin_set_param(Plugin* p, int idx, float norm);
void kf_plugin_set_param_ui(Plugin* p, int idx, float norm);
float kf_plugin_get_param(Plugin* p, int idx);
void kf_plugin_begin_edit(Plugin* p, int idx);
void kf_plugin_end_edit(Plugin* p, int idx);
void kf_plugin_load_preset(Plugin* p, int idx);
void kf_plugin_notify(Plugin* p, int opcode, int index, intptr_t value, void* ptr, float opt);
int kf_plugin_make_chunk(Plugin* p);
int kf_plugin_read_chunk(Plugin* p, const void* data, int size);
int kf_settings_load(Plugin* p);
int kf_settings_save(const Plugin* p);

/* ------------------------------------------------------------------ */
/* Editor                                                              */
/* ------------------------------------------------------------------ */
extern HINSTANCE kf_dll_instance;

void* kf_editor_create(HWND parent, Plugin* p);
void kf_editor_destroy(void* ed);
void kf_editor_tick(void* ed);
void kf_editor_apply_settings(void* ed);
void kf_editor_queue_settings(void* ed);
void kf_editor_detach_all(void);
void kf_editor_get_size(int* w, int* h);
int kf_editor_render_bmp(void* ed, const char* path);
int kf_editor_render_bmp2x(void* ed, const char* path);

static double kf_db2lin(double db) {
    return pow(10.0, db * 0.05);
}

static double kf_clamp(double x, double lo, double hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

#ifdef __cplusplus
}
#endif

#endif
