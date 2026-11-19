/* KickForge - DARKAERO editor, pure Win32 GDI
   Cold chrome-glass interface: layered steel panels, hairline bevels,
   ice-blue accents, procedural aero background. */
#include "kf.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define EW KF_UI_W
#define EH KF_UI_H
static int g_ss = 2;
static int g_panel_index = 0;
static int g_style = 0;
#define SS g_ss

static COLORREF g_cy = RGB(118, 198, 230);
static COLORREF g_mg = RGB(184, 208, 226);
static COLORREF g_ac = RGB(226, 239, 248);
static COLORREF g_or = RGB(96, 148, 180);
static COLORREF g_tx = RGB(206, 221, 234);
static COLORREF g_dim = RGB(128, 151, 170);
static COLORREF g_mut = RGB(58, 78, 95);
static COLORREF g_dk = RGB(6, 11, 17);

#define CY g_cy
#define MG g_mg
#define AC g_ac
#define OR g_or
#define TX g_tx
#define DIM g_dim
#define MUT g_mut
#define DK g_dk

static COLORREF shade(COLORREF c, int pct) {
    return RGB(GetRValue(c) * pct / 100, GetGValue(c) * pct / 100, GetBValue(c) * pct / 100);
}

static COLORREF lighten(COLORREF c, int pct) {
    int r = GetRValue(c) + (255 - GetRValue(c)) * pct / 100;
    int g = GetGValue(c) + (255 - GetGValue(c)) * pct / 100;
    int b = GetBValue(c) + (255 - GetBValue(c)) * pct / 100;
    return RGB(r, g, b);
}

static COLORREF mix_col(COLORREF a, COLORREF b, int pct) {
    return RGB(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * pct / 100,
               GetGValue(a) + (GetGValue(b) - GetGValue(a)) * pct / 100,
               GetBValue(a) + (GetBValue(b) - GetBValue(a)) * pct / 100);
}

enum {
    CT_KNOB = 1,
    CT_BTN,
    CT_TOGGLE,
    CT_MENU,
    CT_PRESET,
    CT_AB
};

enum {
    ACT_PREV = 1,
    ACT_NEXT,
    ACT_RANDOM,
    ACT_INIT,
    ACT_BYPASS,
    ACT_HELP,
    ACT_AB_A,
    ACT_AB_B,
    ACT_SETTINGS,
    ACT_SCALE_100,
    ACT_SCALE_125,
    ACT_SCALE_150,
    ACT_BG,
    ACT_STYLE_0,
    ACT_STYLE_1,
    ACT_THEME_0,
    ACT_THEME_1,
    ACT_THEME_2,
    ACT_THEME_3
};

typedef struct Ctrl {
    int type;
    int id;
    int param;
    int x, y, w, h, r;
    int label_above;
    int compact;
    const char* label;
} Ctrl;

typedef struct Editor {
    HWND hwnd;
    Plugin* plugin;
    struct Editor* next;
    HDC dc;
    HBITMAP dib;
    void* bits;
    HDC odc;
    HBITMAP odib;
    void* obits;
    HFONT f_logo, f_big, f_title, f_label, f_small, f_value, f_mono;
    Ctrl ctrl[64];
    int nctrl;
    int drag_id, drag_param, drag_y;
    float drag_start;
    int hover_id;
    int preset_open, menu_hover, help_open;
    int ab_slot;
    float ab_params[2][NPARAMS];
    int clip_hold;
    float hold_l, hold_r;
    unsigned int rng;
    int out_w, out_h;
    int ss;
    int applied_scale;
    int settings_open;
    HBITMAP bg_bitmap;
    int bg_theme;
    int bg_tried;
} Editor;

static int g_class_reg = 0;
static HINSTANCE g_class_inst = NULL;
static char g_class_name[64];
static Editor* g_editors = NULL;
static void apply_editor_settings(Editor* ed);

/* DARKAERO has two finishes and four accent sets. Style 1 is the
   heavier "Brood" chrome variant; both share the same control layout. */
static void apply_theme(Editor* ed) {
    int th = 0, st = 0;
    if (ed && ed->plugin) {
        th = ed->plugin->ui_theme;
        st = ed->plugin->ui_style;
    }
    if (th < 0) th = 0;
    if (th > 3) th = 3;
    if (st < 0) st = 0;
    if (st > 1) st = 1;

    if (th == 1) {
        g_cy = RGB(238, 104, 192);
        g_mg = RGB(210, 142, 196);
        g_ac = RGB(252, 218, 240);
        g_or = RGB(168, 88, 136);
        g_tx = RGB(226, 210, 222);
        g_dim = RGB(154, 128, 148);
        g_mut = RGB(84, 58, 76);
        g_dk = RGB(10, 6, 12);
    } else if (th == 2) {
        g_cy = RGB(232, 92, 88);
        g_mg = RGB(206, 142, 132);
        g_ac = RGB(252, 220, 212);
        g_or = RGB(166, 78, 74);
        g_tx = RGB(228, 212, 210);
        g_dim = RGB(160, 130, 128);
        g_mut = RGB(88, 58, 60);
        g_dk = RGB(12, 5, 7);
    } else if (th == 3) {
        g_cy = RGB(64, 102, 168);
        g_mg = RGB(108, 136, 176);
        g_ac = RGB(196, 214, 238);
        g_or = RGB(48, 84, 134);
        g_tx = RGB(186, 202, 224);
        g_dim = RGB(104, 126, 154);
        g_mut = RGB(38, 52, 74);
        g_dk = RGB(4, 7, 13);
    } else {
        g_cy = RGB(118, 198, 230);
        g_mg = RGB(184, 208, 226);
        g_ac = RGB(226, 239, 248);
        g_or = RGB(96, 148, 180);
        g_tx = RGB(206, 221, 234);
        g_dim = RGB(128, 151, 170);
        g_mut = RGB(58, 78, 95);
        g_dk = RGB(6, 11, 17);
    }
    if (st == 1) {
        g_dk = RGB(3, 5, 8);
        g_mut = shade(g_mut, 84);
        g_dim = lighten(g_dim, 4);
    }
    g_style = st;
}

/* ------------------------------------------------------------------ */
/* Small drawing helpers                                               */
/* ------------------------------------------------------------------ */
static void fill_rect(HDC dc, int x, int y, int w, int h, COLORREF c) {
    HBRUSH b = CreateSolidBrush(c);
    RECT r;
    r.left = x * SS; r.top = y * SS; r.right = (x + w) * SS; r.bottom = (y + h) * SS;
    FillRect(dc, &r, b);
    DeleteObject(b);
}

static void grad_rect(HDC dc, int x, int y, int w, int h, COLORREF c1, COLORREF c2, int vert) {
    TRIVERTEX vt[2];
    GRADIENT_RECT gr;
    vt[0].x = x * SS; vt[0].y = y * SS;
    vt[0].Red = (COLOR16)(GetRValue(c1) << 8);
    vt[0].Green = (COLOR16)(GetGValue(c1) << 8);
    vt[0].Blue = (COLOR16)(GetBValue(c1) << 8);
    vt[0].Alpha = 0xFF00;
    vt[1].x = (x + w) * SS; vt[1].y = (y + h) * SS;
    vt[1].Red = (COLOR16)(GetRValue(c2) << 8);
    vt[1].Green = (COLOR16)(GetGValue(c2) << 8);
    vt[1].Blue = (COLOR16)(GetBValue(c2) << 8);
    vt[1].Alpha = 0xFF00;
    gr.UpperLeft = 0;
    gr.LowerRight = 1;
    GradientFill(dc, vt, 2, &gr, 1, vert ? GRADIENT_FILL_RECT_V : GRADIENT_FILL_RECT_H);
}

static void line_xy(HDC dc, int x1, int y1, int x2, int y2, COLORREF c, int width) {
    HPEN pen = CreatePen(PS_SOLID, width, c);
    HGDIOBJ old = SelectObject(dc, pen);
    MoveToEx(dc, x1 * SS, y1 * SS, NULL);
    LineTo(dc, x2 * SS, y2 * SS);
    SelectObject(dc, old);
    DeleteObject(pen);
}

static void frame_rect(HDC dc, int x, int y, int w, int h, COLORREF c, int width) {
    HBRUSH nb = (HBRUSH)GetStockObject(NULL_BRUSH);
    HPEN pen = CreatePen(PS_SOLID, width, c);
    HGDIOBJ op = SelectObject(dc, pen);
    HGDIOBJ ob = SelectObject(dc, nb);
    Rectangle(dc, x * SS, y * SS, (x + w) * SS + 1, (y + h) * SS + 1);
    SelectObject(dc, op);
    SelectObject(dc, ob);
    DeleteObject(pen);
}

static void ellipse_fill(HDC dc, int cx, int cy, int rx, int ry, COLORREF c) {
    HBRUSH b = CreateSolidBrush(c);
    HBRUSH nb = (HBRUSH)GetStockObject(NULL_BRUSH);
    HGDIOBJ ob = SelectObject(dc, b);
    HGDIOBJ op = SelectObject(dc, nb);
    Ellipse(dc, (cx - rx) * SS, (cy - ry) * SS, (cx + rx) * SS + 1, (cy + ry) * SS + 1);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(b);
}

static void ellipse_ring(HDC dc, int cx, int cy, int rx, int ry, COLORREF c, int width) {
    HPEN pen = CreatePen(PS_SOLID, width, c);
    HBRUSH nb = (HBRUSH)GetStockObject(NULL_BRUSH);
    HGDIOBJ op = SelectObject(dc, pen);
    HGDIOBJ ob = SelectObject(dc, nb);
    Ellipse(dc, (cx - rx) * SS, (cy - ry) * SS, (cx + rx) * SS + 1, (cy + ry) * SS + 1);
    SelectObject(dc, op);
    SelectObject(dc, ob);
    DeleteObject(pen);
}

static void poly_fill(HDC dc, const POINT* pts, int n, COLORREF c) {
    POINT sp[16];
    HBRUSH b = CreateSolidBrush(c);
    HGDIOBJ ob;
    int i;
    for (i = 0; i < n; i++) { sp[i].x = pts[i].x * SS; sp[i].y = pts[i].y * SS; }
    ob = SelectObject(dc, b);
    Polygon(dc, sp, n);
    SelectObject(dc, ob);
    DeleteObject(b);
}

static void poly_line(HDC dc, const POINT* pts, int n, COLORREF c, int width) {
    POINT sp[16];
    HPEN pen = CreatePen(PS_SOLID, width, c);
    HGDIOBJ op;
    int i;
    for (i = 0; i < n; i++) { sp[i].x = pts[i].x * SS; sp[i].y = pts[i].y * SS; }
    op = SelectObject(dc, pen);
    Polyline(dc, sp, n);
    SelectObject(dc, op);
    DeleteObject(pen);
}

static void text_l(HDC dc, int x, int y, HFONT f, COLORREF c, const char* s) {
    if (!s) return;
    SelectObject(dc, f);
    SetTextColor(dc, c);
    SetBkMode(dc, TRANSPARENT);
    TextOutA(dc, x * SS, y * SS, s, lstrlenA(s));
}

static void text_c(HDC dc, int x, int y, HFONT f, COLORREF c, const char* s) {
    SIZE sz;
    if (!s) return;
    SelectObject(dc, f);
    SetTextColor(dc, c);
    SetBkMode(dc, TRANSPARENT);
    GetTextExtentPoint32A(dc, s, lstrlenA(s), &sz);
    TextOutA(dc, x * SS - sz.cx / 2, y * SS, s, lstrlenA(s));
}

static void text_r(HDC dc, int x, int y, HFONT f, COLORREF c, const char* s) {
    SIZE sz;
    if (!s) return;
    SelectObject(dc, f);
    SetTextColor(dc, c);
    SetBkMode(dc, TRANSPARENT);
    GetTextExtentPoint32A(dc, s, lstrlenA(s), &sz);
    TextOutA(dc, x * SS - sz.cx, y * SS, s, lstrlenA(s));
}

static void hazard_band(HDC dc, int x, int y, int w, int h) {
    int i;
    fill_rect(dc, x, y, w, h, RGB(14, 24, 35));
    fill_rect(dc, x, y, w, 1, RGB(132, 162, 182));
    fill_rect(dc, x, y + h - 1, w, 1, RGB(3, 7, 11));
    if (g_style == 1) {
        if (h >= 12) {
            for (i = 4; i < w - 4; i += 14) {
                int seg = (i / 14) % 3;
                COLORREF sc = (seg == 0) ? RGB(126, 154, 174)
                                        : (seg == 1 ? RGB(22, 35, 47) : RGB(64, 90, 108));
                fill_rect(dc, x + i, y + 3, 10, h - 7, sc);
                fill_rect(dc, x + i, y + 3, 1, h - 7, RGB(198, 220, 232));
            }
        } else {
            for (i = 4; i < w - 4; i += 18) {
                fill_rect(dc, x + i, y + 2, 10, 1, RGB(142, 168, 184));
                fill_rect(dc, x + i + 10, y + 2, 8, 1, RGB(24, 38, 50));
            }
        }
    } else if (h >= 12) {
        for (i = 5; i < w - 4; i += 9) {
            line_xy(dc, x + i, y + 3, x + i, y + h - 4, RGB(60, 88, 108), 2);
            line_xy(dc, x + i + 1, y + 3, x + i + 1, y + h - 4, RGB(22, 36, 50), 1);
        }
    } else {
        line_xy(dc, x + 4, y + h / 2, x + w - 4, y + h / 2, RGB(74, 104, 124), 1);
    }
    fill_rect(dc, x, y, 3, h, CY);
    fill_rect(dc, x + 3, y, 1, h, RGB(202, 228, 240));
}

static void screw(HDC dc, int x, int y, int r) {
    ellipse_fill(dc, x, y, r, r, RGB(5, 9, 14));
    ellipse_fill(dc, x, y, r - 1, r - 1, RGB(28, 44, 57));
    ellipse_ring(dc, x, y, r - 1, r - 1, RGB(134, 160, 178), 1);
    line_xy(dc, x - r + 2, y - r + 2, x + r - 2, y + r - 2, RGB(8, 13, 18), 1);
    line_xy(dc, x - r + 2, y + r - 2, x + r - 2, y - r + 2, RGB(74, 98, 116), 1);
}

static void panel(HDC dc, Editor* ed, int x, int y, int w, int h,
                  const char* title, const char* sub, COLORREF accent) {
    POINT p[8];
    POINT sh[8];
    HRGN rgn;
    int save;
    int i;
    int c = 9;
    int pno = ++g_panel_index;
    char pbuf[4];
    int st = g_style;
    COLORREF body1 = mix_col(RGB(37, 56, 72), CY, st ? 7 : 10);
    COLORREF body2 = st ? RGB(7, 11, 16) : RGB(15, 25, 35);
    COLORREF head1 = st ? RGB(46, 57, 68) : RGB(60, 84, 104);
    COLORREF head2 = st ? RGB(9, 14, 19) : RGB(23, 37, 50);
    p[0].x = x + c; p[0].y = y;
    p[1].x = x + w - c; p[1].y = y;
    p[2].x = x + w; p[2].y = y + c;
    p[3].x = x + w; p[3].y = y + h - c;
    p[4].x = x + w - c; p[4].y = y + h;
    p[5].x = x + c; p[5].y = y + h;
    p[6].x = x; p[6].y = y + h - c;
    p[7].x = x; p[7].y = y + c;
    for (i = 0; i < 8; i++) { sh[i].x = p[i].x + 4; sh[i].y = p[i].y + 5; }

    poly_fill(dc, sh, 8, RGB(2, 5, 9));
    poly_fill(dc, p, 8, st ? RGB(5, 8, 11) : RGB(17, 29, 40));
    poly_line(dc, p, 8, RGB(3, 7, 11), 4);

    /* glass body, clipped to the chamfered silhouette */
    rgn = CreatePolygonRgn(p, 8, WINDING);
    save = SaveDC(dc);
    SelectClipRgn(dc, rgn);
    grad_rect(dc, x + 2, y + 35, w - 4, h - 37, body1, body2, 1);
    grad_rect(dc, x + 4, y + 38, w - 8, 18, lighten(body1, st ? 12 : 18), body1, 1);
    fill_rect(dc, x + 4, y + 35, w - 8, 2, mix_col(RGB(103, 136, 156), CY, st ? 24 : 0));
    fill_rect(dc, x + 4, y + 37, w - 8, 1, RGB(29, 45, 60));
    for (i = 0; i < h - 40; i += 24) {
        line_xy(dc, x + 8, y + 40 + i, x + w - 8, y + 40 + i, RGB(14, 24, 34), 1);
    }
    if (st == 1) {
        for (i = x + 18; i < x + w - 12; i += 18)
            line_xy(dc, i, y + 40, i, y + h - 12, RGB(9, 15, 21), 1);
        line_xy(dc, x + 12, y + 46, x + w - 12, y + 46, mix_col(RGB(52, 74, 90), CY, 16), 1);
    }
    RestoreDC(dc, save);
    DeleteObject(rgn);

    /* header plate */
    grad_rect(dc, x + 2, y + 2, w - 4, 30, head1, head2, 1);
    if (st == 1) {
        grad_rect(dc, x + 2, y + 2, 32, 30, RGB(58, 72, 84), RGB(15, 22, 29), 1);
        fill_rect(dc, x + 34, y + 2, 1, 30, RGB(150, 176, 192));
        fill_rect(dc, x + 2, y + 24, w - 82, 2, RGB(12, 18, 24));
        for (i = 0; i < 11; i++)
            fill_rect(dc, x + 8 + i * 18, y + 26, 7, 2, i % 2 ? RGB(91, 116, 132) : RGB(28, 42, 54));
        for (i = 0; i < (w - 90) / 18; i++)
            fill_rect(dc, x + 4 + i * 18, y + 2, 8, 2,
                      i % 2 ? RGB(142, 166, 182) : RGB(24, 38, 50));
    } else {
        fill_rect(dc, x + 2, y + 25, w - 82, 1, RGB(32, 48, 62));
    }
    hazard_band(dc, x + w - 78, y + 2, 76, 30);
    fill_rect(dc, x + 2, y + 2, w - 4, 1, RGB(178, 205, 221));
    fill_rect(dc, x + 2, y + 31, w - 4, 2, RGB(3, 7, 11));
    line_xy(dc, x + 2, y + 33, x + w - 2, y + 33, accent, 1);
    line_xy(dc, x + 2, y + 34, x + w - 2, y + 34, shade(accent, 42), 1);

    sprintf(pbuf, "%02d", pno);
    text_l(dc, x + 10, y + 9, ed->f_small, RGB(144, 172, 190), pbuf);
    line_xy(dc, x + 30, y + 8, x + 30, y + 24, RGB(104, 138, 158), 1);
    text_l(dc, x + 39, y + 8, ed->f_title, RGB(226, 238, 247), title);
    if (sub) {
        SIZE ts, ss;
        int sx, title_end, limit, sw, show = 1;
        SelectObject(dc, ed->f_title);
        GetTextExtentPoint32A(dc, title, lstrlenA(title), &ts);
        SelectObject(dc, ed->f_small);
        GetTextExtentPoint32A(dc, sub, lstrlenA(sub), &ss);
        title_end = x + 39 + ts.cx / SS;
        limit = x + w - 92;
        sw = ss.cx / SS;
        sx = title_end + 12;
        if (sx + sw > limit) {
            if (limit - sw >= title_end + 8) sx = limit - sw;
            else show = 0;
        }
        if (show)
            text_l(dc, sx, y + 11, ed->f_small, DIM, sub);
    }

    /* inner hairline + bevels */
    frame_rect(dc, x + 6, y + 38, w - 12, h - 46, RGB(51, 76, 94), 1);
    line_xy(dc, x + 7, y + 39, x + w - 7, y + 39, RGB(92, 122, 142), 1);
    line_xy(dc, x + 7, y + h - 8, x + w - 7, y + h - 8, RGB(4, 9, 14), 1);
    line_xy(dc, x + 10, y + 1, x + w - 10, y + 1, RGB(142, 170, 188), 1);
    line_xy(dc, x + 1, y + 12, x + 1, y + h - 12, RGB(76, 102, 120), 1);
    screw(dc, x + 8, y + 8, 3);
    screw(dc, x + w - 8, y + 8, 3);
    screw(dc, x + w - 8, y + h - 8, 3);
}

/* ------------------------------------------------------------------ */
/* Knobs, buttons, meters, scope                                       */
/* ------------------------------------------------------------------ */
static void polar(int cx, int cy, int r, double deg, int* ox, int* oy) {
    double a = deg * 3.14159265358979323846 / 180.0;
    *ox = cx + (int)(cos(a) * (double)r);
    *oy = cy + (int)(sin(a) * (double)r);
}

static void arc_draw(HDC dc, int cx, int cy, int r, double a0, double a1, COLORREF c, int width) {
    POINT pts[66];
    double step;
    int i, n = 64;
    int sx = cx * SS, sy = cy * SS, sr = r * SS;
    if (a1 < a0) { double t = a0; a0 = a1; a1 = t; }
    step = (a1 - a0) / (double)n;
    for (i = 0; i <= n; i++) {
        int px, py;
        polar(sx, sy, sr, a0 + step * i, &px, &py);
        pts[i].x = px; pts[i].y = py;
    }
    {
        HPEN pen = CreatePen(PS_SOLID, width, c);
        HGDIOBJ op = SelectObject(dc, pen);
        Polyline(dc, pts, n + 1);
        SelectObject(dc, op);
        DeleteObject(pen);
    }
}

static void draw_knob(Editor* ed, HDC dc, Ctrl* c) {
    Plugin* p = ed->plugin;
    int idx = c->param;
    float v = p->params[idx];
    const ParamDef* d = &kf_param_defs[idx];
    int cx = c->x, cy = c->y, r = c->r;
    int active = (ed->drag_id == c->id) || (ed->hover_id == c->id);
    COLORREF accent = active ? RGB(206, 234, 247) : CY;
    int i;
    char val[64];
    int lx, ly;

    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    kf_param_format(idx, v, val, sizeof(val));

    /* recessed socket, brushed chrome collar, black glass cap */
    ellipse_fill(dc, cx + 2, cy + 3, r + 6, r + 6, RGB(2, 5, 8));
    ellipse_fill(dc, cx, cy, r + 5, r + 5, RGB(130, 156, 174));
    ellipse_ring(dc, cx, cy, r + 5, r + 5, RGB(198, 220, 232), 1);
    arc_draw(dc, cx, cy, r + 4, 198.0, 340.0, RGB(228, 242, 250), 2);
    ellipse_fill(dc, cx, cy, r + 3, r + 3, RGB(24, 38, 51));
    ellipse_fill(dc, cx, cy, r, r, RGB(10, 19, 28));
    ellipse_ring(dc, cx, cy, r, r, active ? accent : RGB(93, 121, 141), 1);
    arc_draw(dc, cx, cy, r - 3, 205.0, 252.0, RGB(76, 104, 122), 3);
    arc_draw(dc, cx, cy, r - 6, 208.0, 246.0, RGB(38, 60, 76), 2);
    arc_draw(dc, cx, cy, r - 1, 198.0, 316.0, RGB(132, 164, 184), 1);

    /* fine tick ring */
    for (i = 0; i <= 10; i++) {
        int x1, y1, x2, y2;
        double a = 135.0 + 27.0 * i;
        polar(cx, cy, r + 7, a, &x1, &y1);
        polar(cx, cy, (i == 5) ? r + 12 : r + 10, a, &x2, &y2);
        line_xy(dc, x1, y1, x2, y2,
                (i == 5) ? RGB(172, 196, 212) : RGB(68, 92, 110),
                (i == 5) ? 2 : 1);
    }
    if (g_style == 1) {
        for (i = 0; i < 32; i++) {
            int x1, y1, x2, y2;
            double a = (double)i * 11.25;
            polar(cx, cy, r + 4, a, &x1, &y1);
            polar(cx, cy, r + (i % 2 ? 8 : 6), a, &x2, &y2);
            line_xy(dc, x1, y1, x2, y2, i % 2 ? RGB(88, 112, 130) : RGB(26, 40, 52), 1);
        }
    }

    /* value slot */
    arc_draw(dc, cx, cy, r - 6, 135.0, 405.0, RGB(30, 47, 62), 3);
    {
        double a = 135.0 + 270.0 * (double)v;
        double start = d->bipolar ? 270.0 : 135.0;
        if (fabs(a - start) > 1.5) {
            arc_draw(dc, cx, cy, r - 6, start, a, accent, 3);
        }
    }

    /* pointer: a thin machined needle, not a neon bar */
    {
        double a = 135.0 + 270.0 * (double)v;
        int x1, y1, x2, y2;
        polar(cx, cy, (int)(r * 0.18), a, &x1, &y1);
        polar(cx, cy, r - 10, a, &x2, &y2);
        line_xy(dc, x1 + 1, y1 + 2, x2 + 1, y2 + 2, RGB(0, 0, 0), 4);
        line_xy(dc, x1, y1, x2, y2, RGB(232, 244, 252), 2);
        ellipse_fill(dc, x2, y2, 2, 2, accent);
    }
    ellipse_fill(dc, cx, cy, 4, 4, RGB(6, 12, 18));
    ellipse_ring(dc, cx, cy, 4, 4, RGB(142, 172, 190), 1);

    if (c->compact) {
        char line[128];
        sprintf(line, "%s %s", d->shortname, val);
        text_c(dc, cx, cy + r + 5, ed->f_small, active ? RGB(238, 247, 253) : TX, line);
    } else if (c->label_above) {
        lx = cx;
        ly = cy - r - 30;
        text_c(dc, lx, ly, ed->f_label, active ? accent : DIM, d->shortname);
        text_c(dc, lx, ly + 15, ed->f_value, active ? RGB(255, 255, 255) : TX, val);
    } else {
        lx = cx;
        ly = cy + r + 6;
        text_c(dc, lx, ly, ed->f_label, active ? accent : DIM, d->shortname);
        text_c(dc, lx, ly + 22, ed->f_value, active ? RGB(255, 255, 255) : TX, val);
    }
}

static void cut_rect(HDC dc, int x, int y, int w, int h, int cut, COLORREF fillc, COLORREF borderc, int bw) {
    POINT p[8];
    p[0].x = x + cut; p[0].y = y;
    p[1].x = x + w - cut; p[1].y = y;
    p[2].x = x + w; p[2].y = y + cut;
    p[3].x = x + w; p[3].y = y + h - cut;
    p[4].x = x + w - cut; p[4].y = y + h;
    p[5].x = x + cut; p[5].y = y + h;
    p[6].x = x; p[6].y = y + h - cut;
    p[7].x = x; p[7].y = y + cut;
    poly_fill(dc, p, 8, fillc);
    poly_line(dc, p, 8, borderc, bw);
}

static void draw_toggle(Editor* ed, HDC dc, Ctrl* c) {
    Plugin* p = ed->plugin;
    int on = p->params[c->param] > 0.5f;
    int active = ed->hover_id == c->id || ed->drag_id == c->id;
    COLORREF ac = (c->param == P_BYPASS) ? RGB(212, 112, 106) : CY;
    char st[16];
    POINT p8[8];
    int i;
    p8[0].x = c->x + 5; p8[0].y = c->y;
    p8[1].x = c->x + c->w - 5; p8[1].y = c->y;
    p8[2].x = c->x + c->w; p8[2].y = c->y + 5;
    p8[3].x = c->x + c->w; p8[3].y = c->y + c->h - 5;
    p8[4].x = c->x + c->w - 5; p8[4].y = c->y + c->h;
    p8[5].x = c->x + 5; p8[5].y = c->y + c->h;
    p8[6].x = c->x; p8[6].y = c->y + c->h - 5;
    p8[7].x = c->x; p8[7].y = c->y + 5;
    poly_fill(dc, p8, 8, on ? RGB(18, 42, 55) : RGB(13, 23, 32));
    poly_line(dc, p8, 8, on ? ac : (active ? RGB(150, 180, 198) : RGB(62, 86, 103)), 1);
    line_xy(dc, c->x + 5, c->y + 1, c->x + c->w - 5, c->y + 1,
            on ? RGB(92, 154, 178) : RGB(68, 92, 110), 1);
    for (i = 0; i < 3; i++) {
        fill_rect(dc, c->x + 9 + i * 4, c->y + 10 + i * 4, 2, 2,
                  on ? ac : RGB(44, 64, 80));
    }
    sprintf(st, "%s", on ? "ON" : "OFF");
    text_l(dc, c->x + 25, c->y + 6, ed->f_small, on ? RGB(232, 244, 252) : DIM, c->label);
    text_r(dc, c->x + c->w - 8, c->y + 6, ed->f_small, on ? ac : MUT, st);
}

static void draw_menu(Editor* ed, HDC dc, Ctrl* c) {
    Plugin* p = ed->plugin;
    char val[64];
    int active = ed->hover_id == c->id;
    kf_param_format(c->param, p->params[c->param], val, sizeof(val));
    cut_rect(dc, c->x, c->y, c->w, c->h, 7, RGB(10, 18, 26),
             active ? CY : RGB(66, 92, 110), 1);
    line_xy(dc, c->x + 7, c->y + 1, c->x + c->w - 7, c->y + 1,
            active ? RGB(126, 176, 198) : RGB(74, 100, 118), 1);
    line_xy(dc, c->x + 7, c->y + c->h - 1, c->x + c->w - 7, c->y + c->h - 1,
            RGB(3, 7, 11), 1);
    if (c->h >= 38) {
        text_l(dc, c->x + 11, c->y + 5, ed->f_small, DIM, c->label);
        text_c(dc, c->x + c->w / 2, c->y + 16, ed->f_big,
               active ? RGB(238, 248, 253) : RGB(206, 230, 242), val);
        text_r(dc, c->x + c->w - 14, c->y + 18, ed->f_title, active ? CY : MUT, "v");
    } else if (c->h >= 30) {
        text_l(dc, c->x + 9, c->y + 1, ed->f_small, DIM, c->label);
        text_c(dc, c->x + c->w / 2, c->y + 17, ed->f_value,
               active ? RGB(238, 248, 253) : RGB(206, 230, 242), val);
    } else {
        text_c(dc, c->x + c->w / 2, c->y + 5, ed->f_value,
               active ? RGB(238, 248, 253) : TX, val);
        text_c(dc, c->x + c->w / 2, c->y + c->h - 12, ed->f_small, DIM, c->label);
    }
}

static void draw_btn(Editor* ed, HDC dc, Ctrl* c) {
    int active = ed->hover_id == c->id;
    int on = 0;
    COLORREF ac = CY;
    COLORREF border = active ? CY : RGB(66, 92, 110);
    if (c->id == ACT_BYPASS) {
        on = ed->plugin->params[P_BYPASS] > 0.5f;
        ac = RGB(212, 112, 106);
        if (on) border = ac;
    }
    cut_rect(dc, c->x, c->y, c->w, c->h, 6,
             on ? RGB(42, 24, 28) : (active ? RGB(29, 46, 60) : RGB(14, 25, 35)),
             border, 1);
    line_xy(dc, c->x + 6, c->y + 1, c->x + c->w - 6, c->y + 1,
            on ? RGB(126, 74, 74) : (active ? RGB(124, 170, 192) : RGB(72, 98, 116)), 1);
    line_xy(dc, c->x + 6, c->y + c->h - 1, c->x + c->w - 6, c->y + c->h - 1,
            RGB(3, 7, 11), 1);
    if (on) fill_rect(dc, c->x + 6, c->y + c->h - 4, c->w - 12, 2, ac);
    if (c->id == ACT_SETTINGS) {
        text_c(dc, c->x + c->w / 2, c->y + (c->h - 12) / 2, ed->f_small,
               active ? RGB(255, 255, 255) : TX, c->label);
    } else {
        text_c(dc, c->x + c->w / 2, c->y + (c->h - 12) / 2, ed->f_title,
               on ? RGB(245, 214, 214) : (active ? RGB(255, 255, 255) : TX), c->label);
    }
}

static void draw_ab(Editor* ed, HDC dc, Ctrl* c) {
    int sel = (c->id == ACT_AB_A) ? (ed->ab_slot == 0) : (ed->ab_slot == 1);
    int active = ed->hover_id == c->id;
    cut_rect(dc, c->x, c->y, c->w, c->h, 5,
             sel ? RGB(18, 44, 58) : RGB(12, 22, 31),
             sel ? CY : (active ? RGB(150, 180, 198) : RGB(62, 86, 103)), 1);
    text_c(dc, c->x + c->w / 2, c->y + 7, ed->f_title,
           sel ? RGB(224, 242, 251) : DIM, c->label);
}

static void draw_preset_box(Editor* ed, HDC dc, Ctrl* c) {
    Plugin* p = ed->plugin;
    int active = ed->hover_id == c->id || ed->preset_open;
    char name[96];
    const char* nm;
    const char* tag;
    if (p->current_preset >= 0 && p->current_preset < KF_NUM_PRESETS && !p->preset_dirty) {
        nm = kf_presets[p->current_preset].name;
        tag = kf_presets[p->current_preset].tag;
    } else {
        nm = "CUSTOM / EDITED";
        tag = "EDIT";
    }
    sprintf(name, "%s%s", nm, p->preset_dirty ? " *" : "");
    cut_rect(dc, c->x, c->y, c->w, c->h, 6, RGB(10, 18, 26),
             active ? CY : RGB(64, 90, 108), 1);
    fill_rect(dc, c->x + 1, c->y + 8, 3, c->h - 16, CY);
    line_xy(dc, c->x + 7, c->y + 1, c->x + c->w - 7, c->y + 1, RGB(76, 104, 122), 1);
    text_l(dc, c->x + 12, c->y + 3, ed->f_small, DIM, "PRESET");
    text_l(dc, c->x + 12, c->y + 16, ed->f_title, active ? RGB(255, 255, 255) : TX, name);
    text_r(dc, c->x + c->w - 32, c->y + 16, ed->f_small, CY, tag);
    {
        POINT p3[3];
        p3[0].x = c->x + c->w - 18; p3[1].x = c->x + c->w - 8; p3[2].x = c->x + c->w - 13;
        p3[0].y = c->y + 15; p3[1].y = c->y + 15; p3[2].y = c->y + 22;
        poly_fill(dc, p3, 3, active ? CY : DIM);
    }
}

static void meter_color(int i, int n, COLORREF* c) {
    if (i > n - 3) *c = RGB(210, 104, 100);
    else if (i > n * 2 / 3) *c = g_ac;
    else if (i > n / 3) *c = g_cy;
    else *c = shade(g_cy, 62);
}

static void draw_meter_v(HDC dc, Editor* ed, int x, int y, int w, int h, float peak, float hold, const char* lab) {
    int n = 22;
    int i;
    int gap = 1;
    int seg = (h - gap * (n - 1)) / n;
    float db = 20.0f * (float)log10((double)peak + 1.0e-7);
    float hn = (db + 60.0f) / 60.0f;
    float hdb = 20.0f * (float)log10((double)hold + 1.0e-7);
    float hh = (hdb + 60.0f) / 60.0f;
    COLORREF c;
    if (hn < 0.0f) hn = 0.0f;
    if (hn > 1.0f) hn = 1.0f;
    if (hh < 0.0f) hh = 0.0f;
    if (hh > 1.0f) hh = 1.0f;
    text_c(dc, x + w / 2, y - 13, ed->f_small, DIM, lab);
    for (i = 0; i < n; i++) {
        int sy = y + h - (i + 1) * seg - i * gap;
        int on = ((float)i / (float)n) < hn;
        meter_color(i, n, &c);
        fill_rect(dc, x, sy, w, seg, on ? c : RGB(24, 28, 34));
    }
    if (hh > 0.02f) {
        int hy = y + h - (int)(hh * h);
        fill_rect(dc, x - 2, hy, w + 4, 2, RGB(255, 255, 255));
    }
}

static void draw_meter_h(HDC dc, Editor* ed, int x, int y, int w, int h, float peak, float hold, const char* lab) {
    int n = 30;
    int i;
    int gap = 1;
    int seg = (w - gap * (n - 1)) / n;
    float db = 20.0f * (float)log10((double)peak + 1.0e-7);
    float hn = (db + 60.0f) / 60.0f;
    float hdb = 20.0f * (float)log10((double)hold + 1.0e-7);
    float hh = (hdb + 60.0f) / 60.0f;
    COLORREF c;
    if (hn < 0.0f) hn = 0.0f;
    if (hn > 1.0f) hn = 1.0f;
    if (hh < 0.0f) hh = 0.0f;
    if (hh > 1.0f) hh = 1.0f;
    text_r(dc, x - 6, y - 1, ed->f_small, DIM, lab);
    for (i = 0; i < n; i++) {
        int sx = x + i * (seg + gap);
        int on = ((float)i / (float)n) < hn;
        meter_color(i, n, &c);
        fill_rect(dc, sx, y, seg, h, on ? c : RGB(24, 28, 34));
    }
    if (hh > 0.02f) {
        int hx = x + (int)(hh * w);
        fill_rect(dc, hx, y - 2, 2, h + 4, RGB(255, 255, 255));
    }
}

static void draw_scope(Editor* ed, HDC dc) {
    Plugin* p = ed->plugin;
    int x = 40, y = 394, w = 468, h = 148;
    int i, j;
    float buf[KF_SCOPE_SIZE];
    float peak = 0.0f;
    int pos = p->scope_pos;
    POINT pts[513];
    int npts = 512;

    fill_rect(dc, x, y, w, h, RGB(4, 10, 16));
    for (i = 0; i <= 8; i++) {
        line_xy(dc, x + i * w / 8, y, x + i * w / 8, y + h, RGB(18, 32, 44), 1);
    }
    for (i = 0; i <= 4; i++) {
        line_xy(dc, x, y + i * h / 4, x + w, y + i * h / 4, RGB(18, 32, 44), 1);
    }
    line_xy(dc, x, y + h / 2, x + w, y + h / 2, shade(CY, 30), 1);
    frame_rect(dc, x, y, w, h, RGB(58, 84, 102), 1);

    for (i = 0; i < KF_SCOPE_SIZE; i++) {
        float v = p->scope[(pos + i) & (KF_SCOPE_SIZE - 1)];
        buf[i] = v;
        if (fabs((double)v) > (double)peak) peak = (float)fabs((double)v);
    }
    if (peak < 0.08f) peak = 0.08f;
    {
        float scale = 0.44f * (float)h / peak;
        int step = KF_SCOPE_SIZE / npts;
        if (step < 1) step = 1;
        for (j = 0; j < npts; j++) {
            float sum = 0.0f;
            int k;
            for (k = 0; k < step; k++) sum += buf[j * step + k];
            sum /= (float)step;
            pts[j].x = (x + (j * w) / (npts - 1)) * SS;
            pts[j].y = (int)((float)(y + h / 2) * SS - sum * scale * SS);
        }
        {
            HPEN glow = CreatePen(PS_SOLID, 9, shade(CY, 16));
            HPEN mid = CreatePen(PS_SOLID, 4, shade(CY, 62));
            HPEN core = CreatePen(PS_SOLID, 1, lighten(CY, 58));
            HGDIOBJ op;
            op = SelectObject(dc, glow); Polyline(dc, pts, npts);
            SelectObject(dc, mid); Polyline(dc, pts, npts);
            SelectObject(dc, core); Polyline(dc, pts, npts);
            SelectObject(dc, op);
            DeleteObject(glow); DeleteObject(mid); DeleteObject(core);
        }
    }
    {
        char info[64];
        float pdb = 20.0f * (float)log10((double)p->peak_l + 1.0e-7);
        sprintf(info, "42 ms  //  PEAK %.1f dB", pdb);
        fill_rect(dc, x + 2, y + 2, 190, 19, RGB(4, 10, 16));
        fill_rect(dc, x + w - 106, y + 2, 104, 19, RGB(4, 10, 16));
        text_l(dc, x + 8, y + 7, ed->f_small, CY, info);
        text_r(dc, x + w - 8, y + 7, ed->f_small, DIM, "MONO SUM");
        if (ed->clip_hold > 0) {
            fill_rect(dc, x + w - 62, y + h - 19, 54, 12, RGB(58, 24, 28));
            text_c(dc, x + w - 35, y + h - 18, ed->f_small, RGB(244, 186, 182), "CLIP");
        }
    }
}

/* ------------------------------------------------------------------ */
/* Header, preset overlay, help                                        */
/* ------------------------------------------------------------------ */
static void load_background(Editor* ed) {
    (void)ed;
}

static void draw_background(Editor* ed, HDC dc) {
    int i, x, y;
    int st = ed->plugin ? ed->plugin->ui_style : 0;
    COLORREF bg1 = mix_col(RGB(18, 31, 43), CY, 8);
    COLORREF bg2 = mix_col(RGB(4, 8, 13), CY, 3);
    COLORREF bg3 = mix_col(RGB(34, 54, 71), CY, 10);
    COLORREF grid = mix_col(RGB(25, 42, 56), CY, 8);
    if (st == 1) {
        bg1 = mix_col(RGB(11, 18, 25), CY, 10);
        bg2 = RGB(2, 4, 7);
        bg3 = mix_col(RGB(22, 34, 44), CY, 12);
    }

    /* cold aero atmosphere: deep blue-black glass, no bitmap background */
    grad_rect(dc, 0, 0, EW, EH, bg1, bg2, 1);
    grad_rect(dc, 0, 0, EW, 180, bg3, bg1, 1);

    /* broad diagonal light sheets */
    {
        int save = SaveDC(dc);
        IntersectClipRect(dc, 0, 0, EW * SS, EH * SS);
        for (i = -320; i < EW + 320; i += 210) {
            HPEN pen = CreatePen(PS_SOLID, 46 * SS, mix_col(RGB(22, 39, 54), CY, 8));
            HGDIOBJ op = SelectObject(dc, pen);
            MoveToEx(dc, i * SS, 0, NULL);
            LineTo(dc, (i + 360) * SS, EH * SS);
            SelectObject(dc, op);
            DeleteObject(pen);
        }
        for (i = -160; i < EW + 160; i += 210) {
            HPEN pen = CreatePen(PS_SOLID, 1 * SS, mix_col(RGB(42, 68, 88), CY, 18));
            HGDIOBJ op = SelectObject(dc, pen);
            MoveToEx(dc, i * SS, 0, NULL);
            LineTo(dc, (i + 360) * SS, EH * SS);
            SelectObject(dc, op);
            DeleteObject(pen);
        }
        RestoreDC(dc, save);
    }

    if (st == 1) {
        /* Brood finish: dense hex mesh and hard chrome slashes */
        for (y = 34; y < EH; y += 46) {
            for (x = 12; x < EW; x += 54) {
                POINT hx[6];
                int k, ox = x + ((y / 46) & 1) * 27;
                for (k = 0; k < 6; k++) {
                    double a = 3.14159265358979323846 / 3.0 * k;
                    hx[k].x = ox + (int)(cos(a) * 21.0);
                    hx[k].y = y + (int)(sin(a) * 21.0);
                }
                poly_line(dc, hx, 6, mix_col(RGB(24, 38, 50), CY, 8), 1);
            }
        }
        for (i = 0; i < 5; i++) {
            int yy = 90 + i * 132;
            line_xy(dc, 0, yy, EW, yy - 48, mix_col(RGB(44, 68, 86), CY, 12), 1);
            line_xy(dc, 0, yy + 2, EW, yy - 46, RGB(10, 17, 24), 1);
        }
    }

    /* precision engineering grid */
    for (i = 0; i < EW; i += 24) line_xy(dc, i, 0, i, EH, grid, 1);
    for (i = 0; i < EH; i += 24) line_xy(dc, 0, i, EW, i, grid, 1);

    /* chrome arcs bleeding in from the upper-right, like the reference hero */
    {
        int save = SaveDC(dc);
        IntersectClipRect(dc, 0, 0, EW * SS, 150 * SS);
        for (i = 0; i < 7; i++) {
            int rr = 72 + i * 18;
            ellipse_ring(dc, 1018, 8, rr, rr,
                         i == 0 ? mix_col(RGB(58, 86, 106), CY, 22)
                                : mix_col(RGB(30, 50, 66), CY, 10), 1);
        }
        RestoreDC(dc, save);
    }

    /* upper-left glass wing */
    {
        POINT wing[5];
        wing[0].x = 0; wing[0].y = 0;
        wing[1].x = 352; wing[1].y = 0;
        wing[2].x = 292; wing[2].y = 84;
        wing[3].x = 0; wing[3].y = 84;
        wing[4].x = 0; wing[4].y = 0;
        poly_fill(dc, wing, 4, mix_col(RGB(26, 43, 58), CY, 8));
        poly_line(dc, wing, 4, mix_col(RGB(55, 82, 101), CY, 18), 1);
    }

    /* faint scanlines and edge shading */
    for (i = 0; i < EH; i += 4) line_xy(dc, 0, i, EW, i, RGB(8, 15, 22), 1);
    grad_rect(dc, 0, 640, EW, 34, RGB(8, 15, 22), RGB(3, 6, 10), 1);
    frame_rect(dc, 0, 0, EW, EH, mix_col(RGB(52, 78, 96), CY, 20), 1);
    line_xy(dc, 0, 84, EW, 84, mix_col(RGB(82, 112, 132), CY, 25), 1);
}

static void draw_header(Editor* ed, HDC dc) {
    SIZE sz;
    int fx;
    POINT wing[5];
    int st = g_style;
    int i;

    grad_rect(dc, 0, 0, EW, 84,
              mix_col(RGB(44, 66, 84), CY, st ? 10 : 14),
              mix_col(RGB(9, 16, 24), CY, 4), 1);
    wing[0].x = 0; wing[0].y = 0;
    wing[1].x = 356; wing[1].y = 0;
    wing[2].x = 300; wing[2].y = 84;
    wing[3].x = 0; wing[3].y = 84;
    poly_fill(dc, wing, 4, mix_col(RGB(31, 50, 66), CY, st ? 8 : 12));
    poly_line(dc, wing, 4, mix_col(RGB(64, 92, 112), CY, st ? 26 : 18), 1);
    fill_rect(dc, 0, 0, EW, 5, RGB(6, 11, 17));
    hazard_band(dc, 0, 0, EW, 5);
    if (st == 1) {
        for (i = 0; i < 13; i++) {
            fill_rect(dc, 742 + i * 25, 8, 18, 2, i % 2 ? RGB(108, 134, 150) : RGB(26, 40, 52));
            fill_rect(dc, 742 + i * 25, 10, 18, 1, RGB(6, 10, 14));
        }
    }
    line_xy(dc, 0, 84, EW, 84, RGB(72, 102, 122), 2);
    line_xy(dc, 0, 86, EW, 86, shade(CY, 66), 1);

    SelectObject(dc, ed->f_logo);
    GetTextExtentPoint32A(dc, "KICK", 4, &sz);
    fx = 30 + sz.cx / SS + 10;
    text_l(dc, 33, 19, ed->f_logo, RGB(3, 8, 13), "KICK");
    text_l(dc, 30, 16, ed->f_logo, RGB(229, 240, 248), "KICK");
    text_l(dc, fx + 2, 19, ed->f_logo, RGB(3, 8, 13), "FORGE");
    text_l(dc, fx, 16, ed->f_logo, CY, "FORGE");
    text_l(dc, 32, 63, ed->f_small, DIM, "H A R D C O R E   K I C K   D I S T O R T I O N");

    text_r(dc, 1046, 17, ed->f_title, RGB(230, 241, 249), "DARKAERO ENGINE");
    text_r(dc, 1046, 38, ed->f_small, CY, "VST2 / 64-BIT / FL STUDIO");
    text_r(dc, 1046, 55, ed->f_small, DIM, "KICK DESIGN STATION // v2.1.2");
    screw(dc, 1050, 69, 4);

    line_xy(dc, 700, 14, 700, 68, RGB(79, 108, 128), 1);
    line_xy(dc, 701, 14, 701, 68, RGB(16, 27, 38), 1);
    fill_rect(dc, 30, 60, 168, 1, RGB(84, 116, 136));
    fill_rect(dc, 30, 61, 168, 1, RGB(12, 21, 30));
}

static void draw_preset_menu(Editor* ed, HDC dc) {
    int x = 70, y = 306, w = 402, h = 10 * 28 + 10;
    int i;
    fill_rect(dc, x + 5, y + 6, w, h, RGB(2, 3, 5));
    fill_rect(dc, x, y, w, h, RGB(12, 14, 18));
    frame_rect(dc, x, y, w, h, CY, 2);
    text_l(dc, x + 12, y + 6, ed->f_small, DIM, "FACTORY PRESETS  //  CLICK TO LOAD");
    for (i = 0; i < KF_NUM_PRESETS; i++) {
        int ry = y + 24 + i * 28;
        int hot = (ed->menu_hover == i);
        if (hot) fill_rect(dc, x + 5, ry - 2, w - 10, 26, shade(CY, 24));
        if (i == ed->plugin->current_preset) fill_rect(dc, x + 5, ry - 2, 4, 26, AC);
        {
            char num[8], nm[96];
            sprintf(num, "%02d", i + 1);
            sprintf(nm, "%s", kf_presets[i].name);
            text_l(dc, x + 14, ry + 4, ed->f_mono, hot ? CY : MUT, num);
            text_l(dc, x + 42, ry + 2, ed->f_title, hot ? RGB(255, 255, 255) : TX, nm);
            text_r(dc, x + w - 14, ry + 6, ed->f_small, hot ? AC : DIM, kf_presets[i].tag);
        }
    }
}

static void draw_help(Editor* ed, HDC dc) {
    int x = 212, y = 116, w = 656, h = 468;
    int i;
    static const char* lines[] = {
        "KICKFORGE is a three-band nonlinear kick forge.",
        "",
        "1. Start with a preset, then push INPUT and DRIVE.",
        "2. 3-BAND FORGE: LOW = sub, MID = body, HIGH = click.",
        "3. PUNCH tightens the transient. BODY adds low weight.",
        "4. CHAR selects the distortion curve. BIAS adds asymmetry.",
        "5. OS 4x / 8x tames aliasing. MONO keeps the sub centered.",
        "6. AUTO evens level. CEILING is the final brickwall.",
        "",
        "DRAG knob = adjust     SHIFT+DRAG = fine",
        "DOUBLE-CLICK / RIGHT-CLICK = reset to default",
        "A / B = compare two states     RANDOM = controlled chaos",
        "",
        "Tip: low drive + 100% mid mix is the classic hardstyle stack.",
        "For frenchcore, try FOLD or GABBER char with high band drive."
    };
    fill_rect(dc, x + 6, y + 7, w, h, RGB(2, 5, 9));
    grad_rect(dc, x, y, w, h, RGB(29, 47, 62), RGB(8, 14, 21), 1);
    frame_rect(dc, x, y, w, h, RGB(94, 136, 160), 1);
    hazard_band(dc, x, y, w, 8);
    text_l(dc, x + 22, y + 22, ed->f_big, RGB(226, 239, 248), "CONTROL INDEX / QUICK START");
    text_r(dc, x + w - 34, y + 20, ed->f_small, DIM, "CLICK X TO CLOSE");
    line_xy(dc, x + 20, y + 56, x + w - 20, y + 56, RGB(102, 150, 174), 1);
    for (i = 0; i < (int)(sizeof(lines) / sizeof(lines[0])); i++) {
        COLORREF c = (i < 2) ? TX : (i == 8 ? DIM : RGB(178, 196, 210));
        if (i == 0) c = CY;
        text_l(dc, x + 24, y + 70 + i * 24, ed->f_label, c, lines[i]);
    }
    cut_rect(dc, x + w - 40, y + 14, 24, 24, 4, RGB(42, 22, 26), RGB(198, 122, 118), 1);
    text_c(dc, x + w - 28, y + 18, ed->f_title, RGB(242, 214, 212), "X");
}

static void draw_footer(Editor* ed, HDC dc) {
    char buf[96];
    grad_rect(dc, 0, 674, EW, 26, RGB(17, 29, 40), RGB(5, 10, 16), 1);
    line_xy(dc, 0, 674, EW, 674, RGB(72, 102, 122), 1);
    line_xy(dc, 0, 675, EW, 675, RGB(7, 13, 20), 1);
    text_l(dc, 26, 681, ed->f_small, DIM, "KICKFORGE // DARKAERO");
    sprintf(buf, "ENGINE 3-BAND LR4  //  SR %.1f kHz  //  2 IN / 2 OUT", (double)ed->plugin->sr / 1000.0);
    text_c(dc, EW / 2, 681, ed->f_small, MUT, buf);
    text_r(dc, 1054, 681, ed->f_small, DIM, "XDC-AM  //  DARKAERO UI 2.1.2");
}

/* ------------------------------------------------------------------ */
/* Layout and UI actions                                               */
/* ------------------------------------------------------------------ */
static Ctrl* add_ctrl(Editor* ed, int type, int id, int param, int x, int y, int w, int h, int r, const char* label) {
    Ctrl* c = &ed->ctrl[ed->nctrl++];
    memset(c, 0, sizeof(*c));
    c->type = type;
    c->id = id;
    c->param = param;
    c->x = x; c->y = y; c->w = w; c->h = h; c->r = r;
    c->label = label;
    return c;
}

static Ctrl* add_knob(Editor* ed, int param, int x, int y, int r) {
    Ctrl* c = add_ctrl(ed, CT_KNOB, param + 100, param, x, y, r * 2, r * 2, r, NULL);
    c->compact = 1;
    return c;
}

static void layout(Editor* ed) {
    Ctrl* c;
    ed->nctrl = 0;

    add_knob(ed, P_INPUT, 80, 178, 34);
    add_knob(ed, P_DRIVE, 182, 178, 46);
    add_knob(ed, P_MIX, 290, 178, 34);
    c = add_knob(ed, P_BIAS, 78, 286, 16); c->label_above = 0;
    add_ctrl(ed, CT_MENU, 200, P_CHAR, 116, 274, 240, 46, 0, "CHARACTER");

    add_knob(ed, P_LOW_DRIVE, 452, 158, 24);
    add_knob(ed, P_MID_DRIVE, 584, 158, 24);
    add_knob(ed, P_HIGH_DRIVE, 716, 158, 24);
    add_knob(ed, P_LOW_MIX, 452, 226, 18);
    add_knob(ed, P_MID_MIX, 584, 226, 18);
    add_knob(ed, P_HIGH_MIX, 716, 226, 18);
    add_knob(ed, P_SPLIT_LOW, 488, 292, 14);
    add_knob(ed, P_SPLIT_HIGH, 666, 292, 14);

    add_knob(ed, P_PUNCH, 854, 164, 30);
    add_knob(ed, P_BODY, 972, 164, 30);
    add_knob(ed, P_TONE, 854, 254, 30);
    add_knob(ed, P_CRUSH, 972, 254, 30);

    add_ctrl(ed, CT_TOGGLE, 400 + P_AUTO, P_AUTO, 544, 390, 94, 28, 0, "AUTO");
    add_ctrl(ed, CT_TOGGLE, 400 + P_MONO, P_MONO, 646, 390, 94, 28, 0, "MONO");
    add_ctrl(ed, CT_MENU, 500 + P_OS, P_OS, 748, 386, 124, 36, 0, "OVERSAMPLE");
    add_ctrl(ed, CT_MENU, 500 + P_CLIP, P_CLIP, 880, 386, 80, 36, 0, "CLIP");

    add_knob(ed, P_OUTPUT, 580, 468, 25);
    add_knob(ed, P_CEILING, 668, 468, 25);
    add_knob(ed, P_RATE, 756, 468, 25);
    add_knob(ed, P_WIDTH, 844, 468, 25);
    add_knob(ed, P_LOWCUT, 932, 468, 25);

    add_ctrl(ed, CT_BTN, ACT_PREV, -1, 36, 616, 28, 32, 0, "<");
    add_ctrl(ed, CT_PRESET, 0, -1, 70, 616, 280, 34, 0, NULL);
    add_ctrl(ed, CT_BTN, ACT_NEXT, -1, 356, 616, 28, 32, 0, ">");
    add_ctrl(ed, CT_AB, ACT_AB_A, -1, 398, 616, 36, 32, 0, "A");
    add_ctrl(ed, CT_AB, ACT_AB_B, -1, 438, 616, 36, 32, 0, "B");
    add_ctrl(ed, CT_BTN, ACT_RANDOM, -1, 486, 616, 88, 32, 0, "RANDOM");
    add_ctrl(ed, CT_BTN, ACT_INIT, -1, 582, 616, 64, 32, 0, "INIT");
    add_ctrl(ed, CT_BTN, ACT_BYPASS, -1, 654, 616, 88, 32, 0, "BYPASS");
    add_ctrl(ed, CT_BTN, ACT_HELP, -1, 750, 616, 58, 32, 0, "HELP");
    add_ctrl(ed, CT_BTN, ACT_SETTINGS, -1, 884, 590, 74, 20, 0, "UI SETUP");
}

static Ctrl* hit_ctrl(Editor* ed, int x, int y) {
    int i;
    for (i = ed->nctrl - 1; i >= 0; i--) {
        Ctrl* c = &ed->ctrl[i];
        if (c->type == CT_KNOB) {
            int dx = x - c->x;
            int dy = y - c->y;
            int rr = c->r + 9;
            if (dx * dx + dy * dy <= rr * rr) return c;
        } else {
            if (x >= c->x && x <= c->x + c->w && y >= c->y && y <= c->y + c->h) return c;
        }
    }
    return NULL;
}

static void map_mouse(Editor* ed, int* x, int* y) {
    if (!ed || ed->out_w <= 0 || ed->out_h <= 0) return;
    *x = *x * EW / ed->out_w;
    *y = *y * EH / ed->out_h;
    if (*x < 0) *x = 0;
    if (*y < 0) *y = 0;
    if (*x >= EW) *x = EW - 1;
    if (*y >= EH) *y = EH - 1;
}

static float rnd01(Editor* ed) {    ed->rng = ed->rng * 1664525u + 1013904223u;
    return (float)((ed->rng >> 8) & 0xFFFF) / 65535.0f;
}

static void ui_set(Editor* ed, int idx, float v) {
    const ParamDef* d = &kf_param_defs[idx];
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    if (d->discrete) {
        float steps = d->max - d->min;
        if (steps > 0.0f) v = (float)floor(v * steps + 0.5) / steps;
    }
    kf_plugin_set_param_ui(ed->plugin, idx, v);
}

static void preset_load(Editor* ed, int idx) {
    int i;
    kf_plugin_load_preset(ed->plugin, idx);
    for (i = 0; i < NPARAMS; i++) {
        kf_plugin_notify(ed->plugin, audioMasterAutomate, i, 0, NULL, ed->plugin->params[i]);
    }
    kf_plugin_notify(ed->plugin, audioMasterUpdateDisplay, 0, 0, NULL, 0.0f);
    for (i = 0; i < NPARAMS; i++) ed->ab_params[ed->ab_slot][i] = ed->plugin->params[i];
    ed->plugin->preset_dirty = 0;
}

static void do_random(Editor* ed) {
    Plugin* p = ed->plugin;
    ui_set(ed, P_INPUT, 0.35f + rnd01(ed) * 0.25f);
    ui_set(ed, P_DRIVE, 0.30f + rnd01(ed) * 0.45f);
    ui_set(ed, P_MIX, 0.75f + rnd01(ed) * 0.25f);
    ui_set(ed, P_CHAR, rnd01(ed));
    ui_set(ed, P_BIAS, rnd01(ed));
    ui_set(ed, P_LOW_DRIVE, rnd01(ed) * 0.45f);
    ui_set(ed, P_MID_DRIVE, 0.15f + rnd01(ed) * 0.55f);
    ui_set(ed, P_HIGH_DRIVE, 0.20f + rnd01(ed) * 0.65f);
    ui_set(ed, P_LOW_MIX, rnd01(ed) * 0.7f);
    ui_set(ed, P_MID_MIX, 0.65f + rnd01(ed) * 0.35f);
    ui_set(ed, P_HIGH_MIX, 0.65f + rnd01(ed) * 0.35f);
    ui_set(ed, P_SPLIT_LOW, 0.15f + rnd01(ed) * 0.45f);
    ui_set(ed, P_SPLIT_HIGH, 0.35f + rnd01(ed) * 0.45f);
    ui_set(ed, P_PUNCH, rnd01(ed));
    ui_set(ed, P_BODY, 0.25f + rnd01(ed) * 0.6f);
    ui_set(ed, P_TONE, rnd01(ed));
    ui_set(ed, P_CRUSH, rnd01(ed) * 0.35f);
    ui_set(ed, P_RATE, rnd01(ed) * 0.12f);
    ui_set(ed, P_WIDTH, 0.35f + rnd01(ed) * 0.3f);
    ui_set(ed, P_MONO, rnd01(ed) > 0.35f ? 1.0f : 0.0f);
    ui_set(ed, P_LOWCUT, 0.0f + rnd01(ed) * 0.3f);
    ui_set(ed, P_AUTO, rnd01(ed) > 0.5f ? 1.0f : 0.0f);
    ui_set(ed, P_OS, rnd01(ed) > 0.55f ? 1.0f : 2.0f);
    ui_set(ed, P_OUTPUT, 0.45f + rnd01(ed) * 0.2f);
    ui_set(ed, P_CEILING, 0.72f + rnd01(ed) * 0.25f);
    ui_set(ed, P_CLIP, rnd01(ed) > 0.5f ? 1.0f : 0.0f);
    ui_set(ed, P_BYPASS, 0.0f);
    p->current_preset = -1;
    p->preset_dirty = 1;
}

static void ab_switch(Editor* ed, int slot) {
    int i;
    if (slot == ed->ab_slot) return;
    for (i = 0; i < NPARAMS; i++) ed->ab_params[ed->ab_slot][i] = ed->plugin->params[i];
    ed->ab_slot = slot;
    for (i = 0; i < NPARAMS; i++) kf_plugin_set_param_ui(ed->plugin, i, ed->ab_params[slot][i]);
    ed->plugin->current_preset = -1;
    ed->plugin->preset_dirty = 1;
}

static void do_action(Editor* ed, int id) {
    Plugin* p = ed->plugin;
    int idx;
    switch (id) {
    case ACT_PREV:
        idx = (p->current_preset <= 0) ? (KF_NUM_PRESETS - 1) : (p->current_preset - 1);
        preset_load(ed, idx);
        break;
    case ACT_NEXT:
        idx = (p->current_preset < 0 || p->current_preset >= KF_NUM_PRESETS - 1) ? 0 : (p->current_preset + 1);
        preset_load(ed, idx);
        break;
    case ACT_RANDOM:
        do_random(ed);
        break;
    case ACT_INIT:
        preset_load(ed, 0);
        break;
    case ACT_BYPASS:
        kf_plugin_set_param_ui(p, P_BYPASS, p->params[P_BYPASS] > 0.5f ? 0.0f : 1.0f);
        break;
    case ACT_HELP:
        ed->help_open = !ed->help_open;
        ed->settings_open = 0;
        break;
    case ACT_SETTINGS:
        ed->settings_open = !ed->settings_open;
        ed->help_open = 0;
        ed->preset_open = 0;
        break;
    case ACT_SCALE_100:
        p->ui_scale = 0; apply_editor_settings(ed); break;
    case ACT_SCALE_125:
        p->ui_scale = 1; apply_editor_settings(ed); break;
    case ACT_SCALE_150:
        p->ui_scale = 2; apply_editor_settings(ed); break;
    case ACT_STYLE_0:
        p->ui_style = 0; apply_editor_settings(ed); break;
    case ACT_STYLE_1:
        p->ui_style = 1; apply_editor_settings(ed); break;
    case ACT_THEME_0:
        p->ui_theme = 0; apply_editor_settings(ed); break;
    case ACT_THEME_1:
        p->ui_theme = 1; apply_editor_settings(ed); break;
    case ACT_THEME_2:
        p->ui_theme = 2; apply_editor_settings(ed); break;
    case ACT_THEME_3:
        p->ui_theme = 3; apply_editor_settings(ed); break;
    case ACT_AB_A:
        ab_switch(ed, 0);
        break;
    case ACT_AB_B:
        ab_switch(ed, 1);
        break;
    default:
        break;
    }
    if (id >= ACT_SCALE_100 && id <= ACT_THEME_3)
        kf_settings_save(p);
}

/* ------------------------------------------------------------------ */
/* UI setup overlay                                                    */
/* ------------------------------------------------------------------ */
static int settings_hit(Editor* ed, int x, int y) {
    if (!ed->settings_open) return 0;
    if (x >= 842 && x <= 900 && y >= 148 && y <= 174) return ACT_SETTINGS;
    if (y >= 226 && y <= 264) {
        if (x >= 190 && x <= 302) return ACT_SCALE_100;
        if (x >= 314 && x <= 426) return ACT_SCALE_125;
        if (x >= 438 && x <= 550) return ACT_SCALE_150;
    }
    if (y >= 304 && y <= 360) {
        if (x >= 190 && x <= 460) return ACT_STYLE_0;
        if (x >= 500 && x <= 770) return ACT_STYLE_1;
    }
    if (y >= 402 && y <= 440) {
        if (x >= 190 && x <= 320) return ACT_THEME_0;
        if (x >= 340 && x <= 470) return ACT_THEME_1;
        if (x >= 490 && x <= 620) return ACT_THEME_2;
        if (x >= 640 && x <= 770) return ACT_THEME_3;
    }
    return 0;
}

static void settings_button(HDC dc, Editor* ed, int x, int y, int w, int h,
                            const char* label, int active, COLORREF accent) {
    cut_rect(dc, x, y, w, h, 6, active ? RGB(20, 45, 59) : RGB(12, 22, 31),
             active ? accent : RGB(66, 92, 110), 1);
    line_xy(dc, x + 6, y + 1, x + w - 6, y + 1,
            active ? RGB(124, 174, 198) : RGB(72, 98, 116), 1);
    if (active) fill_rect(dc, x + 6, y + h - 3, w - 12, 2, accent);
    text_c(dc, x + w / 2, y + (h - 14) / 2, ed->f_small,
           active ? RGB(240, 249, 254) : TX, label);
}

static void draw_settings(Editor* ed) {
    Plugin* p = ed->plugin;
    int i;
    const char* scalename[3];
    const char* tname[4];
    COLORREF tcol[4];
    int sx[3];
    int tx[4];
    int active_scale, active_style, active_theme;

    scalename[0] = "100%"; scalename[1] = "125%"; scalename[2] = "150%";
    sx[0] = 190; sx[1] = 314; sx[2] = 438;
    tname[0] = "ORIGINAL"; tname[1] = "MAGENTA"; tname[2] = "CRIMSON"; tname[3] = "INK BLUE";
    tcol[0] = RGB(118, 198, 230);
    tcol[1] = RGB(238, 104, 192);
    tcol[2] = RGB(232, 92, 88);
    tcol[3] = RGB(78, 132, 186);
    tx[0] = 190; tx[1] = 340; tx[2] = 490; tx[3] = 640;
    active_scale = p->ui_scale;
    active_style = p->ui_style;
    active_theme = p->ui_theme;

    fill_rect(ed->dc, 0, 0, EW, EH, RGB(3, 6, 10));
    grad_rect(ed->dc, 160, 135, 760, 430, RGB(29, 47, 62), RGB(6, 11, 17), 1);
    frame_rect(ed->dc, 160, 135, 760, 430, RGB(104, 146, 170), 1);
    hazard_band(ed->dc, 160, 135, 760, 6);
    line_xy(ed->dc, 160, 190, 920, 190, RGB(62, 88, 106), 1);
    text_l(ed->dc, 184, 142, ed->f_big, RGB(226, 239, 248), "DARKAERO / UI SETUP");
    settings_button(ed->dc, ed, 842, 148, 58, 26, "CLOSE", 0, RGB(120, 150, 170));

    text_l(ed->dc, 190, 206, ed->f_small, DIM, "INTERFACE SCALE");
    for (i = 0; i < 3; i++) {
        settings_button(ed->dc, ed, sx[i], 226, 112, 38, scalename[i],
                        active_scale == i, CY);
    }

    text_l(ed->dc, 190, 284, ed->f_small, DIM, "DARKAERO FINISH");
    for (i = 0; i < 2; i++) {
        int x = 190 + i * 310;
        int active = active_style == i;
        cut_rect(ed->dc, x, 304, 270, 56, 7,
                 active ? RGB(17, 35, 46) : RGB(8, 14, 20),
                 active ? CY : RGB(62, 88, 106), 1);
        fill_rect(ed->dc, x + 1, 305, 3, 54, active ? CY : RGB(40, 58, 72));
        text_l(ed->dc, x + 14, 312, ed->f_small, active ? AC : MUT,
               i == 0 ? "STYLE 01  //  CHROME GLASS" : "STYLE 02  //  BROOD CHROME");
        text_l(ed->dc, x + 14, 332, ed->f_title, active ? RGB(240, 249, 254) : TX,
               i == 0 ? "COLD CHROME / GLASS" : "BLACK CHROME / SEGMENTED");
    }

    text_l(ed->dc, 190, 382, ed->f_small, DIM, "ACCENT COLOR");
    for (i = 0; i < 4; i++) {
        int active = active_theme == i;
        cut_rect(ed->dc, tx[i], 402, 130, 38, 5,
                 active ? RGB(16, 30, 40) : RGB(8, 13, 19),
                 active ? tcol[i] : RGB(58, 82, 98), 1);
        fill_rect(ed->dc, tx[i] + 1, 403, 128, 4, tcol[i]);
        text_c(ed->dc, tx[i] + 65, 419, ed->f_small,
               active ? RGB(242, 250, 255) : DIM, tname[i]);
        if (active) fill_rect(ed->dc, tx[i] + 6, 435, 118, 2, tcol[i]);
    }

    text_l(ed->dc, 190, 458, ed->f_small, MUT,
           "Style, color and scale are stored in the VST chunk.");
    text_l(ed->dc, 190, 476, ed->f_small, MUT,
           "No image background is loaded; the aero plate is procedural.");

    text_r(ed->dc, 896, 490, ed->f_title, CY, "XDC-AM");
    text_r(ed->dc, 896, 508, ed->f_small, MUT, "KICKFORGE // DARKAERO UI 2.1.2");
    screw(ed->dc, 176, 149, 4);
}

/* ------------------------------------------------------------------ */
/* Rendering                                                           */
/* ------------------------------------------------------------------ */
static void render(Editor* ed) {
    HDC dc = ed->dc;
    Plugin* p = ed->plugin;
    if (getenv("KF_UI_SETTINGS")) ed->settings_open = 1;
    if (ed->ss) g_ss = ed->ss;
    apply_theme(ed);
    int i;

    draw_background(ed, dc);
    draw_header(ed, dc);
    g_panel_index = 0;

    panel(dc, ed, 24, 92, 360, 250, "INPUT STAGE", "PRE / DRIVE / MIX", CY);
    panel(dc, ed, 396, 92, 376, 250, "3-BAND FORGE", "DRIVE / MIX / SPLIT", MG);
    panel(dc, ed, 784, 92, 272, 250, "SHAPE & COLOR", "NONLINEAR", AC);
    panel(dc, ed, 24, 356, 500, 216, "KICK SCOPE", "42 ms WINDOW", CY);
    panel(dc, ed, 540, 356, 516, 216, "OUTPUT & UTILITY", "FINAL STAGE", OR);
    panel(dc, ed, 24, 586, 1046, 88, "PRESET MEMORY", "10 FACTORY / A-B / RANDOM", CY);

    text_c(dc, 452, 130, ed->f_small, MUT, "LOW");
    text_c(dc, 584, 130, ed->f_small, MUT, "MID");
    text_c(dc, 716, 130, ed->f_small, MUT, "HIGH");

    draw_scope(ed, dc);

    {
        int save = SaveDC(dc);
        IntersectClipRect(dc, 0, 92 * SS, EW * SS, EH * SS);
        for (i = 0; i < ed->nctrl; i++) {
            Ctrl* c = &ed->ctrl[i];
            switch (c->type) {
            case CT_KNOB: draw_knob(ed, dc, c); break;
            case CT_BTN: draw_btn(ed, dc, c); break;
            case CT_TOGGLE: draw_toggle(ed, dc, c); break;
            case CT_MENU: draw_menu(ed, dc, c); break;
            case CT_PRESET: draw_preset_box(ed, dc, c); break;
            case CT_AB: draw_ab(ed, dc, c); break;
            default: break;
            }
        }
        RestoreDC(dc, save);
    }

    draw_meter_v(dc, ed, 1002, 398, 16, 130, p->peak_l, ed->hold_l, "L");
    draw_meter_v(dc, ed, 1026, 398, 16, 130, p->peak_r, ed->hold_r, "R");
    text_l(dc, 998, 540, ed->f_small, MUT, "PEAK");

    draw_meter_h(dc, ed, 866, 618, 190, 10, p->peak_l, ed->hold_l, "L");
    draw_meter_h(dc, ed, 866, 636, 190, 10, p->peak_r, ed->hold_r, "R");
    text_l(dc, 866, 650, ed->f_small, MUT, "-60            -24            -12      0 dB");

    if (ed->drag_id && ed->drag_param >= 0) {
        char buf[96];
        char val[64];
        kf_param_format(ed->drag_param, p->params[ed->drag_param], val, sizeof(val));
        sprintf(buf, "%s  //  %s", kf_param_defs[ed->drag_param].name, val);
        text_c(dc, EW / 2, 660, ed->f_small, CY, buf);
    }

    draw_footer(ed, dc);

    if (ed->preset_open) draw_preset_menu(ed, dc);
    if (ed->help_open) draw_help(ed, dc);
    if (ed->settings_open) draw_settings(ed);

    SetStretchBltMode(ed->odc, HALFTONE);
    SetBrushOrgEx(ed->odc, 0, 0, NULL);
    StretchBlt(ed->odc, 0, 0, ed->out_w, ed->out_h,
               ed->dc, 0, 0, EW * SS, EH * SS, SRCCOPY);
}

/* ------------------------------------------------------------------ */
/* Window proc                                                         */
/* ------------------------------------------------------------------ */
static int hit_help_close(Editor* ed, int x, int y) {
    return ed->help_open && x >= 212 + 656 - 40 && x <= 212 + 656 - 16 && y >= 130 && y <= 154;
}

static int preset_menu_row(Editor* ed, int x, int y) {
    int mx = 70, my = 306, mw = 402, mh = 10 * 28 + 10;
    if (!ed->preset_open) return -1;
    if (x < mx || x > mx + mw || y < my || y > my + mh) return -1;
    if (y < my + 24) return -1;
    {
        int row = (y - (my + 24)) / 28;
        if (row >= 0 && row < KF_NUM_PRESETS) return row;
    }
    return -1;
}

static LRESULT CALLBACK ed_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Editor* ed = (Editor*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    if (msg == WM_CREATE) {
        CREATESTRUCTA* cs = (CREATESTRUCTA*)lp;
        ed = (Editor*)cs->lpCreateParams;
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)ed);
        ed->hwnd = hwnd;
        return 0;
    }
    if (!ed) return DefWindowProcA(hwnd, msg, wp, lp);

    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        render(ed);
        BitBlt(dc, 0, 0, ed->out_w, ed->out_h, ed->odc, 0, 0, SRCCOPY);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_TIMER:
        ed->hold_l = (ed->plugin->peak_l > ed->hold_l) ? ed->plugin->peak_l : ed->hold_l * 0.985f;
        ed->hold_r = (ed->plugin->peak_r > ed->hold_r) ? ed->plugin->peak_r : ed->hold_r * 0.985f;
        if (ed->plugin->clip_led > 0.5f) ed->clip_hold = 48;
        else if (ed->clip_hold > 0) ed->clip_hold--;
        if (IsWindowVisible(hwnd)) InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_APP + 7:
        apply_editor_settings(ed);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_MOUSEMOVE: {
        int x = (short)LOWORD(lp);
        int y = (short)HIWORD(lp);
        map_mouse(ed, &x, &y);
        if (ed->drag_id && ed->drag_param >= 0) {
            int dy = ed->drag_y - y;
            float fine = (GetKeyState(VK_SHIFT) & 0x8000) ? 0.0012f : 0.0055f;
            float v = ed->drag_start + (float)dy * fine;
            ui_set(ed, ed->drag_param, v);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        {
            Ctrl* c = hit_ctrl(ed, x, y);
            int id = c ? c->id : 0;
            int mh = preset_menu_row(ed, x, y);
            if (id != ed->hover_id || mh != ed->menu_hover) {
                ed->hover_id = id;
                ed->menu_hover = mh;
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        int x = (short)LOWORD(lp);
        int y = (short)HIWORD(lp);
        Ctrl* c;
        map_mouse(ed, &x, &y);
        SetFocus(hwnd);
        if (ed->settings_open) {
            int act = settings_hit(ed, x, y);
            if (act) do_action(ed, act);
            else ed->settings_open = 0;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        if (ed->help_open) {
            if (hit_help_close(ed, x, y)) ed->help_open = 0;
            else ed->help_open = 0;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        if (ed->preset_open) {
            int row = preset_menu_row(ed, x, y);
            if (row >= 0) preset_load(ed, row);
            ed->preset_open = 0;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        c = hit_ctrl(ed, x, y);
        if (!c) return 0;
        if (c->type == CT_KNOB) {
            ed->drag_id = c->id;
            ed->drag_param = c->param;
            ed->drag_y = y;
            ed->drag_start = ed->plugin->params[c->param];
            SetCapture(hwnd);
        } else if (c->type == CT_TOGGLE) {
            ui_set(ed, c->param, ed->plugin->params[c->param] > 0.5f ? 0.0f : 1.0f);
        } else if (c->type == CT_MENU) {
            float real = kf_param_norm_to_real(c->param, ed->plugin->params[c->param]);
            int v = (int)floor(real + 0.5) + 1;
            int n = kf_param_defs[c->param].nchoices;
            if (n <= 0) n = (int)(kf_param_defs[c->param].max - kf_param_defs[c->param].min) + 1;
            if (v >= n) v = 0;
            ui_set(ed, c->param, kf_param_real_to_norm(c->param, (float)v + kf_param_defs[c->param].min));
        } else if (c->type == CT_PRESET) {
            ed->preset_open = 1;
            ed->help_open = 0;
        } else if (c->type == CT_AB) {
            do_action(ed, c->id);
        } else if (c->type == CT_BTN) {
            do_action(ed, c->id);
        }
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_LBUTTONUP:
        if (ed->drag_id) {
            ed->drag_id = 0;
            ed->drag_param = -1;
            ReleaseCapture();
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_LBUTTONDBLCLK: {
        int x = (short)LOWORD(lp);
        int y = (short)HIWORD(lp);
        Ctrl* c;
        map_mouse(ed, &x, &y);
        c = hit_ctrl(ed, x, y);
        if (c && c->type == CT_KNOB) {
            ui_set(ed, c->param, kf_param_default(c->param));
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (c && c->type == CT_PRESET) {
            ed->preset_open = 1;
        }
        return 0;
    }
    case WM_RBUTTONDOWN: {
        int x = (short)LOWORD(lp);
        int y = (short)HIWORD(lp);
        Ctrl* c;
        map_mouse(ed, &x, &y);
        c = hit_ctrl(ed, x, y);
        if (c && (c->type == CT_KNOB || c->type == CT_TOGGLE || c->type == CT_MENU)) {
            ui_set(ed, c->param, kf_param_default(c->param));
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_MOUSEWHEEL: {
        POINT pt;
        int x, y;
        int delta = (short)HIWORD(wp);
        Ctrl* c;
        pt.x = (short)LOWORD(lp);
        pt.y = (short)HIWORD(lp);
        ScreenToClient(hwnd, &pt);
        x = pt.x; y = pt.y;
        map_mouse(ed, &x, &y);
        c = hit_ctrl(ed, x, y);
        if (c && c->type == CT_KNOB) {
            float v = ed->plugin->params[c->param] + (delta > 0 ? 0.01f : -0.01f);
            ui_set(ed, c->param, v);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) {
            ed->preset_open = 0;
            ed->help_open = 0;
            ed->settings_open = 0;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_DESTROY:
        if (ed->hwnd == hwnd) ed->hwnd = NULL;
        KillTimer(hwnd, 1);
        return 0;
    case WM_NCDESTROY:
        if (ed) {
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, 0);
            if (ed->hwnd == hwnd) ed->hwnd = NULL;
        }
        return DefWindowProcA(hwnd, msg, wp, lp);
    default:
        return DefWindowProcA(hwnd, msg, wp, lp);
    }
}

static void ensure_class(void) {
    WNDCLASSA wc;
    HINSTANCE inst;
    int attempt;
    if (g_class_reg) return;
    inst = kf_dll_instance ? kf_dll_instance : GetModuleHandleA(NULL);
    memset(&wc, 0, sizeof(wc));
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = ed_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    for (attempt = 0; attempt < 8; attempt++) {
        sprintf(g_class_name, "KickForgeEditor_%p_%d", (void*)inst, attempt);
        wc.lpszClassName = g_class_name;
        if (RegisterClassA(&wc)) break;
        UnregisterClassA(g_class_name, inst);
        if (RegisterClassA(&wc)) break;
    }
    g_class_inst = inst;
    g_class_reg = (attempt < 8);
}

static int make_dib(int w, int h, HDC* dc, HBITMAP* bmp, void** bits) {
    BITMAPINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    *bmp = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, bits, NULL, 0);
    if (!*bmp) return 0;
    *dc = CreateCompatibleDC(NULL);
    if (!*dc) { DeleteObject(*bmp); *bmp = NULL; return 0; }
    SelectObject(*dc, *bmp);
    SetBkMode(*dc, TRANSPARENT);
    return 1;
}

static void free_surfaces(Editor* ed) {
    if (ed->dc) { DeleteDC(ed->dc); ed->dc = NULL; }
    if (ed->dib) { DeleteObject(ed->dib); ed->dib = NULL; }
    if (ed->odc) { DeleteDC(ed->odc); ed->odc = NULL; }
    if (ed->odib) { DeleteObject(ed->odib); ed->odib = NULL; }
    ed->bits = NULL;
    ed->obits = NULL;
}

static int alloc_surfaces(Editor* ed) {
    int idx = ed->plugin->ui_scale;
    int pct;
    if (idx < 0) idx = 0;
    if (idx > 2) idx = 2;
    pct = (idx == 0) ? 100 : (idx == 1 ? 125 : 150);
    g_ss = 2;
    ed->ss = g_ss;
    ed->applied_scale = idx;
    ed->out_w = EW * pct / 100;
    ed->out_h = EH * pct / 100;
    if (!make_dib(EW * g_ss, EH * g_ss, &ed->dc, &ed->dib, &ed->bits)) return 0;
    if (!make_dib(ed->out_w, ed->out_h, &ed->odc, &ed->odib, &ed->obits)) {
        free_surfaces(ed);
        return 0;
    }
    ed->plugin->edit_rect.left = 0;
    ed->plugin->edit_rect.top = 0;
    ed->plugin->edit_rect.right = ed->out_w;
    ed->plugin->edit_rect.bottom = ed->out_h;
    return 1;
}

static HFONT make_font(int logical, int weight, const char* face) {
    return CreateFontA(logical * g_ss, 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                       DEFAULT_PITCH | FF_SWISS, face);
}

static void free_fonts(Editor* ed) {
    if (ed->f_logo) DeleteObject(ed->f_logo);
    if (ed->f_big) DeleteObject(ed->f_big);
    if (ed->f_title) DeleteObject(ed->f_title);
    if (ed->f_label) DeleteObject(ed->f_label);
    if (ed->f_small) DeleteObject(ed->f_small);
    if (ed->f_value) DeleteObject(ed->f_value);
    if (ed->f_mono) DeleteObject(ed->f_mono);
    ed->f_logo = ed->f_big = ed->f_title = ed->f_label = NULL;
    ed->f_small = ed->f_value = ed->f_mono = NULL;
}

static void make_fonts(Editor* ed) {
    ed->f_logo = make_font(46, FW_HEAVY, "Bahnschrift SemiBold Condensed");
    ed->f_big = make_font(23, FW_BOLD, "Bahnschrift SemiBold");
    ed->f_title = make_font(17, FW_BOLD, "Bahnschrift SemiBold");
    ed->f_label = make_font(15, FW_NORMAL, "Bahnschrift");
    ed->f_small = make_font(15, FW_NORMAL, "Bahnschrift");
    ed->f_value = make_font(18, FW_BOLD, "Consolas");
    ed->f_mono = make_font(15, FW_NORMAL, "Consolas");
}

static void apply_editor_settings(Editor* ed) {
    Plugin* p;
    if (!ed) return;
    p = ed->plugin;
    if (p->ui_scale < 0) p->ui_scale = 0;
    if (p->ui_scale > 2) p->ui_scale = 2;
    if (p->ui_theme < 0) p->ui_theme = 0;
    if (p->ui_theme > 3) p->ui_theme = 3;
    if (p->ui_style < 0) p->ui_style = 0;
    if (p->ui_style > 1) p->ui_style = 1;
    p->ui_bg = 0;
    free_fonts(ed);
    free_surfaces(ed);
    if (!alloc_surfaces(ed)) return;
    make_fonts(ed);
    apply_theme(ed);
    if (ed->hwnd) {
        SetWindowPos(ed->hwnd, NULL, 0, 0, ed->out_w, ed->out_h,
                     SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
    }
    if (p->host) {
        kf_plugin_notify(p, audioMasterSizeWindow, ed->out_w, ed->out_h, NULL, 0.0f);
    }
}

void kf_editor_apply_settings(void* v) {
    apply_editor_settings((Editor*)v);
}

void kf_editor_queue_settings(void* v) {
    Editor* ed = (Editor*)v;
    if (ed && ed->hwnd) PostMessageA(ed->hwnd, WM_APP + 7, 0, 0);
}

void kf_editor_detach_all(void) {
    Editor* ed = g_editors;
    while (ed) {
        if (ed->hwnd) {
            HWND hwnd = ed->hwnd;
            KillTimer(hwnd, 1);
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, 0);
            SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG_PTR)DefWindowProcA);
            if (GetWindowThreadProcessId(hwnd, NULL) == GetCurrentThreadId())
                DestroyWindow(hwnd);
            ed->hwnd = NULL;
        }
        ed = ed->next;
    }
    g_editors = NULL;
    if (g_class_reg) {
        UnregisterClassA(g_class_name, g_class_inst);
        g_class_reg = 0;
    }
}

void* kf_editor_create(HWND parent, Plugin* p) {
    Editor* ed;
    HINSTANCE inst;
    DWORD style;
    if (!p) return NULL;
    ed = (Editor*)calloc(1, sizeof(Editor));
    if (!ed) return NULL;
    ed->plugin = p;
    ed->rng = 0x51F15EEDu;
    ed->drag_param = -1;
    ed->ab_slot = 0;
    ed->clip_hold = 0;
    ed->bg_theme = -1;
    ed->out_w = EW;
    ed->out_h = EH;

    if (!alloc_surfaces(ed)) { free(ed); return NULL; }
    make_fonts(ed);
    apply_theme(ed);

    layout(ed);
    {
        int i;
        for (i = 0; i < NPARAMS; i++) {
            ed->ab_params[0][i] = p->params[i];
            ed->ab_params[1][i] = p->params[i];
        }
    }
    ensure_class();
    inst = kf_dll_instance ? kf_dll_instance : GetModuleHandleA(NULL);
    style = parent ? (WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS) : WS_POPUP;
    ed->hwnd = CreateWindowExA(0, g_class_name, "KickForge",
                               style, 0, 0, ed->out_w, ed->out_h, parent, NULL, inst, ed);
    if (!ed->hwnd) {
        kf_editor_destroy(ed);
        return NULL;
    }
    ed->next = g_editors;
    g_editors = ed;
    SetTimer(ed->hwnd, 1, 33, NULL);
    return ed;
}

void kf_editor_destroy(void* v) {
    Editor* ed = (Editor*)v;
    Editor** pp;
    if (!ed) return;
    if (ed->hwnd) {
        HWND hwnd = ed->hwnd;
        KillTimer(hwnd, 1);
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, 0);
        SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG_PTR)DefWindowProcA);
        DestroyWindow(hwnd);
        ed->hwnd = NULL;
    }
    for (pp = &g_editors; *pp; pp = &(*pp)->next) {
        if (*pp == ed) { *pp = ed->next; break; }
    }
    free_surfaces(ed);
    free_fonts(ed);
    if (ed->bg_bitmap) DeleteObject(ed->bg_bitmap);
    if (ed->plugin && ed->plugin->editor == ed) ed->plugin->editor = NULL;
    free(ed);
}

void kf_editor_tick(void* v) {
    Editor* ed = (Editor*)v;
    if (!ed) return;
    if (ed->plugin->ui_scale != ed->applied_scale) apply_editor_settings(ed);
    ed->hold_l = (ed->plugin->peak_l > ed->hold_l) ? ed->plugin->peak_l : ed->hold_l * 0.985f;
    ed->hold_r = (ed->plugin->peak_r > ed->hold_r) ? ed->plugin->peak_r : ed->hold_r * 0.985f;
    if (ed->plugin->clip_led > 0.5f) ed->clip_hold = 48;
    else if (ed->clip_hold > 0) ed->clip_hold--;
    if (ed->hwnd && IsWindowVisible(ed->hwnd)) InvalidateRect(ed->hwnd, NULL, FALSE);
}

void kf_editor_get_size(int* w, int* h) {
    if (w) *w = EW;
    if (h) *h = EH;
}

static int write_bmp32(const char* path, const unsigned char* bits, int w, int h) {
    BITMAPFILEHEADER fh;
    BITMAPINFOHEADER ih;
    HANDLE f;
    DWORD written;
    int y;
    int row = w * 4;
    memset(&fh, 0, sizeof(fh));
    memset(&ih, 0, sizeof(ih));
    fh.bfType = 0x4D42;
    fh.bfOffBits = (DWORD)(sizeof(fh) + sizeof(ih));
    fh.bfSize = fh.bfOffBits + (DWORD)row * (DWORD)h;
    ih.biSize = sizeof(ih);
    ih.biWidth = w;
    ih.biHeight = h;
    ih.biPlanes = 1;
    ih.biBitCount = 32;
    ih.biCompression = BI_RGB;
    ih.biSizeImage = (DWORD)row * (DWORD)h;
    f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return 0;
    WriteFile(f, &fh, sizeof(fh), &written, NULL);
    WriteFile(f, &ih, sizeof(ih), &written, NULL);
    for (y = h - 1; y >= 0; y--) {
        WriteFile(f, bits + (size_t)y * (size_t)row, (DWORD)row, &written, NULL);
    }
    CloseHandle(f);
    return 1;
}

int kf_editor_render_bmp(void* v, const char* path) {
    Editor* ed = (Editor*)v;
    if (!ed || !path) return 0;
    render(ed);
    return write_bmp32(path, (const unsigned char*)ed->obits, ed->out_w, ed->out_h);
}

int kf_editor_render_bmp2x(void* v, const char* path) {
    Editor* ed = (Editor*)v;
    if (!ed || !path) return 0;
    render(ed);
    return write_bmp32(path, (const unsigned char*)ed->bits, EW * SS, EH * SS);
}
