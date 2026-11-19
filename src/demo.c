/* KickForge demo renderer: synthesize hardstyle kicks and print them
   through the exact same DSP used by the VST plug-in. */
#include "kf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define SR 48000
#define BEAT 0.4          /* 150 BPM */
#define KICKS 4
#define TAIL 1.6
#define LEN ((int)(SR * (BEAT * (KICKS - 1) + TAIL)))

static unsigned int rng_state = 0x9E3779B9u;

static double frand(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return (double)(rng_state & 0xFFFFFF) / 16777215.0;
}

static void synth_kick(float* out, int n, int start) {
    double phase = 0.0;
    int i;
    for (i = 0; i < n; i++) {
        double t = (double)i / (double)SR;
        double f, amp, body, sub, click, x;
        if (start + i >= LEN) break;
        f = 46.0 + 190.0 * exp(-t * 20.0) + 40.0 * exp(-t * 60.0);
        phase += 6.283185307179586 * f / (double)SR;
        amp = exp(-t * 3.2);
        body = sin(phase);
        sub = sin(phase * 0.5) * exp(-t * 2.0);
        click = (frand() * 2.0 - 1.0) * exp(-t * 220.0) * 0.55;
        click += sin(6.283185307179586 * 2600.0 * t) * exp(-t * 110.0) * 0.25;
        x = body * amp * 0.85 + sub * 0.18 + click;
        out[start + i] += (float)x;
    }
}

static int write_wav(const char* path, const float* l, const float* r, int n, float gain) {
    FILE* f = fopen(path, "wb");
    unsigned char h[44];
    int i;
    unsigned int datasz = (unsigned int)(n * 2 * 2);
    unsigned int riffsz = 36 + datasz;
    unsigned int sr = SR, br = SR * 4, ba = 4, fmt = 16, pcm = 1, ch = 2, bits = 16;
    if (!f) { printf("cannot write %s\n", path); return 0; }
    memcpy(h + 0, "RIFF", 4);
    memcpy(h + 4, &riffsz, 4);
    memcpy(h + 8, "WAVEfmt ", 8);
    memcpy(h + 16, &fmt, 4);
    memcpy(h + 20, &pcm, 2);
    memcpy(h + 22, &ch, 2);
    memcpy(h + 24, &sr, 4);
    memcpy(h + 28, &br, 4);
    memcpy(h + 32, &ba, 2);
    memcpy(h + 34, &bits, 2);
    memcpy(h + 36, "data", 4);
    memcpy(h + 40, &datasz, 4);
    fwrite(h, 1, 44, f);
    for (i = 0; i < n; i++) {
        double a = (double)l[i] * gain;
        double b = (double)r[i] * gain;
        short sl, sr16;
        if (a > 0.999) a = 0.999; if (a < -0.999) a = -0.999;
        if (b > 0.999) b = 0.999; if (b < -0.999) b = -0.999;
        sl = (short)(a * 32767.0);
        sr16 = (short)(b * 32767.0);
        fwrite(&sl, 2, 1, f);
        fwrite(&sr16, 2, 1, f);
    }
    fclose(f);
    return 1;
}

static int render_preset(int preset, const char* path, const float* inL, const float* inR) {
    Plugin* p = (Plugin*)calloc(1, sizeof(Plugin));
    float* outL = (float*)calloc(LEN, sizeof(float));
    float* outR = (float*)calloc(LEN, sizeof(float));
    int off = 0, i;
    double peak = 0.0;
    if (!p || !outL || !outR) return 0;
    kf_plugin_init(p, NULL);
    kf_plugin_set_sr(p, (float)SR);
    kf_plugin_set_block(p, 512);
    kf_plugin_load_preset(p, preset);
    while (off < LEN) {
        int m = LEN - off;
        float* ip[2];
        float* op[2];
        if (m > 512) m = 512;
        ip[0] = (float*)inL + off;
        ip[1] = (float*)inR + off;
        op[0] = outL + off;
        op[1] = outR + off;
        kf_plugin_process(p, ip, op, m);
        off += m;
    }
    for (i = 0; i < LEN; i++) {
        double a = fabs((double)outL[i]);
        double b = fabs((double)outR[i]);
        if (a > peak) peak = a;
        if (b > peak) peak = b;
    }
    {
        float g = (peak > 1e-9) ? (float)(0.89 / peak) : 1.0f;
        /* fade the last 60 ms to avoid a hard end click */
        int fade = (int)(0.06 * SR);
        for (i = LEN - fade; i < LEN; i++) {
            double k = (double)(LEN - i) / (double)fade;
            outL[i] *= (float)k;
            outR[i] *= (float)k;
        }
        printf("preset %02d  peak=%.4f  gain=%.3f  %s\n", preset, peak, g, path);
        if (!write_wav(path, outL, outR, LEN, g)) { kf_plugin_destroy(p); free(outL); free(outR); return 0; }
    }
    kf_plugin_destroy(p);
    free(outL);
    free(outR);
    return 1;
}

int main(void) {
    float* inL = (float*)calloc(LEN, sizeof(float));
    float* inR = (float*)calloc(LEN, sizeof(float));
    double peak = 0.0;
    int k, i;
    static const int presets[] = { 1, 2, 3, 4, 9 };
    static const char* files[] = {
        "demo\\01_hardstyle_punch.wav",
        "demo\\02_rawstyle_sermon.wav",
        "demo\\03_frenchcore_fold.wav",
        "demo\\04_gabber_hammer.wav",
        "demo\\05_total_destruction.wav"
    };
    if (!inL || !inR) return 2;
    for (k = 0; k < KICKS; k++) {
        int start = (int)(k * BEAT * SR);
        synth_kick(inL, (int)(TAIL * SR), start);
    }
    for (i = 0; i < LEN; i++) {
        double a = fabs((double)inL[i]);
        if (a > peak) peak = a;
    }
    if (peak > 1e-9) {
        float g = (float)(0.75 / peak);
        for (i = 0; i < LEN; i++) { inL[i] *= g; inR[i] = inL[i]; }
    }
    if (!write_wav("demo\\00_clean_reference.wav", inL, inR, LEN, 1.0f)) return 3;
    printf("clean reference written\n");
    for (k = 0; k < 5; k++) {
        if (!render_preset(presets[k], files[k], inL, inR)) return 4;
    }
    free(inL);
    free(inR);
    printf("demo render complete\n");
    return 0;
}
