/* KickForge UI / lifecycle stress test (static link, no host needed) */
#include "kf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int finite_audio(Plugin* p) {
    int i;
    for (i = 0; i < 512; i++) {
        if (!(p->dsp.ch[0].bufA[i] == p->dsp.ch[0].bufA[i])) return 0;
    }
    return 1;
}

static int run_one(int idx, int scale, int theme, int style, int bg) {
    Plugin* p = (Plugin*)calloc(1, sizeof(Plugin));
    void* ed;
    float* in[2];
    float* out[2];
    float l[512], r[512];
    int i, blk;
    int ok = 1;
    if (!p) return 0;
    kf_plugin_init(p, NULL);
    p->ui_scale = scale;
    p->ui_theme = theme;
    p->ui_style = style;
    p->ui_bg = bg;
    kf_plugin_set_sr(p, 48000.0f);
    kf_plugin_set_block(p, 512);
    ed = kf_editor_create(NULL, p);
    if (!ed) { kf_plugin_destroy(p); free(p); return 0; }
    p->editor = ed;
    kf_editor_apply_settings(ed);
    for (i = 0; i < 5; i++) {
        p->ui_scale = i % 3;
        p->ui_style = i & 1;
        kf_editor_apply_settings(ed);
        kf_editor_tick(ed);
    }
    p->ui_scale = scale;
    p->ui_style = style;
    kf_editor_apply_settings(ed);
    if (!kf_editor_render_bmp(ed, "build\\stress_ui.bmp")) ok = 0;

    in[0] = l; in[1] = r; out[0] = l; out[1] = r;
    for (blk = 0; blk < 12; blk++) {
        for (i = 0; i < 512; i++) {
            float t = (float)(blk * 512 + i) / 48000.0f;
            float v = (float)(sin(2.0 * 3.14159265358979 * 55.0 * t) * exp(-t * 4.0));
            l[i] = v; r[i] = v;
        }
        kf_plugin_process(p, in, out, 512);
        if (!finite_audio(p)) ok = 0;
    }
    if (kf_plugin_make_chunk(p) < 20) ok = 0;
    p->ui_scale = 2; p->ui_theme = 3; p->ui_style = 1; p->ui_bg = 0;
    kf_plugin_make_chunk(p);
    if (!kf_plugin_read_chunk(p, p->chunk, p->chunk_size)) ok = 0;
    if (p->ui_scale != 2 || p->ui_theme != 3 || p->ui_style != 1 || p->ui_bg != 0) ok = 0;

    kf_editor_destroy(ed);
    p->editor = NULL;
    kf_plugin_destroy(p);
    printf("stress %d scale=%d theme=%d style=%d bg=%d -> %s\n",
           idx, scale, theme, style, bg, ok ? "OK" : "FAIL");
    return ok;
}

int main(void) {
    int i, round;
    int ok = 1;
    DWORD gdi0 = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    for (round = 0; round < 3; round++) {
        for (i = 0; i < 3; i++) {
            if (!run_one(round * 3 + i, i, (round + i) & 3, (round + i) & 1, 0)) ok = 0;
        }
    }
    for (i = 0; i < 20; i++) {
        Plugin* p = (Plugin*)calloc(1, sizeof(Plugin));
        void* ed;
        if (!p) { ok = 0; break; }
        kf_plugin_init(p, NULL);
        p->ui_scale = i % 3;
        p->ui_theme = i % 4;
        p->ui_style = i & 1;
        ed = kf_editor_create(NULL, p);
        if (!ed) { ok = 0; kf_plugin_destroy(p); free(p); break; }
        p->editor = ed;
        kf_editor_tick(ed);
        kf_editor_destroy(ed);
        kf_plugin_destroy(p);
    }
    printf("%s KickForge stress test (GDI delta=%ld)\n", ok ? "PASS" : "FAIL",
           (long)GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) - (long)gdi0);
    return ok ? 0 : 1;
}
