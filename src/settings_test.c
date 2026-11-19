#include "kf.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    const char* path = "build\\settings_test.ini";
    Plugin* p;
    int ok = 1;

    _putenv("KF_SETTINGS_FILE=build\\settings_test.ini");
    remove(path);

    p = (Plugin*)calloc(1, sizeof(Plugin));
    if (!p) return 2;
    kf_plugin_init(p, NULL);
    p->ui_scale = 1;
    p->ui_style = 1;
    p->ui_theme = 2;
    if (!kf_settings_save(p)) {
        printf("FAIL save\n");
        ok = 0;
    }
    kf_plugin_destroy(p);
    p = (Plugin*)calloc(1, sizeof(Plugin));
    if (!p) return 2;
    if (!p) return 2;
    kf_plugin_init(p, NULL);
    printf("loaded scale=%d style=%d theme=%d rect=%ldx%ld\n", p->ui_scale, p->ui_style, p->ui_theme,
           (long)p->edit_rect.right, (long)p->edit_rect.bottom);
    if (p->ui_scale != 1 || p->ui_style != 1 || p->ui_theme != 2) {
        printf("FAIL loaded values\n");
        ok = 0;
    }
    if (p->edit_rect.right != 1350 || p->edit_rect.bottom != 875) {
        printf("FAIL edit rect after settings load\n");
        ok = 0;
    }
    remove(path);
    kf_plugin_destroy(p);

    if (ok) printf("PASS settings persistence\n");
    return ok ? 0 : 1;
}
