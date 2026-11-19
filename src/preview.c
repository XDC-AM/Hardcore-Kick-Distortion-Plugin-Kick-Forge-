#include "kf.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static void feed_demo(Plugin* p) {
    float bl[512], br[512], ol[512], or[512];
    float* in[2];
    float* out[2];
    int b, i;
    int pos = 0;
    in[0] = bl; in[1] = br;
    out[0] = ol; out[1] = or;
    for (b = 0; b < 8; b++) {
        for (i = 0; i < 512; i++) {
            double t = (double)(pos + i) / 48000.0;
            double env = exp(-t * 6.0);
            double s = env * sin(2.0 * 3.14159265358979 * 55.0 * t) * 0.72;
            if (t < 0.006) s += (1.0 - t / 0.006) * 0.55;
            bl[i] = (float)s;
            br[i] = (float)s;
        }
        kf_plugin_process(p, in, out, 512);
        pos += 512;
    }
}

int main(int argc, char** argv) {
    Plugin* p;
    void* ed;
    const char* path = (argc > 1) ? argv[1] : "assets\\ui_preview.bmp";
    p = (Plugin*)calloc(1, sizeof(Plugin));
    if (!p) { printf("out of memory\n"); return 2; }
    kf_plugin_init(p, NULL);
    kf_plugin_set_sr(p, 48000.0f);
    {
        const char* e;
        if ((e = getenv("KF_UI_SCALE")) != NULL) p->ui_scale = atoi(e);
        if ((e = getenv("KF_UI_THEME")) != NULL) p->ui_theme = atoi(e);
        if ((e = getenv("KF_UI_STYLE")) != NULL) p->ui_style = atoi(e);
        if ((e = getenv("KF_UI_BG")) != NULL) p->ui_bg = atoi(e);
    }
    feed_demo(p);
    ed = kf_editor_create(NULL, p);
    if (!ed) { printf("editor create failed\n"); free(p); return 3; }
    kf_editor_tick(ed);
    if (getenv("KF_2X")) {
        if (!kf_editor_render_bmp2x(ed, path)) {
            printf("render2x failed: %s\n", path);
            kf_editor_destroy(ed);
            free(p);
            return 4;
        }
    } else if (!kf_editor_render_bmp(ed, path)) {
        printf("render failed: %s\n", path);
        kf_editor_destroy(ed);
        free(p);
        return 4;
    }
    printf("wrote %s\n", path);
    kf_editor_destroy(ed);
    free(p);
    return 0;
}
