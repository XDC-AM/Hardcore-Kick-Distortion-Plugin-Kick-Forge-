/* Load the built DLL, open its editor in a real host window, capture client pixels. */
#include "vst2.h"
#include "kf.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef AEffect* (*PluginMain)(void*);

static AEffect* g_effect;
static HWND g_child;

static BOOL CALLBACK enum_child(HWND h, LPARAM l) {
    (void)l;
    if (GetParent(h) && !g_child) g_child = h;
    return TRUE;
}

static void click_ctl(HWND h, int x, int y) {
    SendMessageA(h, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
    SendMessageA(h, WM_LBUTTONUP, 0, MAKELPARAM(x, y));
}

static void pump_ms(MSG* msg, int rounds, int ms) {
    int i;
    for (i = 0; i < rounds; i++) {
        int n = 0;
        while (n++ < 32 && PeekMessageA(msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(msg); DispatchMessageA(msg);
        }
        Sleep(ms);
    }
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
    fh.bfOffBits = sizeof(fh) + sizeof(ih);
    fh.bfSize = fh.bfOffBits + row * h;
    ih.biSize = sizeof(ih);
    ih.biWidth = w;
    ih.biHeight = h;
    ih.biPlanes = 1;
    ih.biBitCount = 32;
    ih.biCompression = BI_RGB;
    f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return 0;
    WriteFile(f, &fh, sizeof(fh), &written, NULL);
    WriteFile(f, &ih, sizeof(ih), &written, NULL);
    for (y = h - 1; y >= 0; y--) WriteFile(f, bits + y * row, row, &written, NULL);
    CloseHandle(f);
    return 1;
}

int main(void) {
    HMODULE dll;
    PluginMain mainfn;
    HWND parent;
    HDC pdc, mdc;
    HBITMAP bmp, old;
    BITMAPINFO bi;
    void* bits = NULL;
    RECT rc;
    MSG msg;
    int i;

    dll = LoadLibraryA("dist\\KickForge.dll");
    if (!dll) { printf("load failed %lu\n", (unsigned long)GetLastError()); return 1; }
    mainfn = (PluginMain)GetProcAddress(dll, "VSTPluginMain");
    if (!mainfn) { printf("no VSTPluginMain\n"); return 1; }
    g_effect = mainfn(NULL);
    if (!g_effect) { printf("no effect\n"); return 1; }
    g_effect->dispatcher(g_effect, effOpen, 0, 0, NULL, 0.0f);
    g_effect->dispatcher(g_effect, effSetSampleRate, 0, 0, NULL, 48000.0f);
    g_effect->dispatcher(g_effect, effSetBlockSize, 0, 512, NULL, 0.0f);
    g_effect->dispatcher(g_effect, effMainsChanged, 0, 1, NULL, 0.0f);

    parent = CreateWindowExA(0, "STATIC", "KickForge capture", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                             20, 20, 1500, 950, NULL, NULL, GetModuleHandleA(NULL), NULL);
    if (!parent) { printf("parent failed\n"); return 1; }
    ShowWindow(parent, SW_SHOW);
    if (g_effect->dispatcher(g_effect, effEditOpen, 0, 0, parent, 0.0f) != 1) {
        printf("edit open failed\n"); return 1;
    }
    EnumChildWindows(parent, enum_child, 0);
    if (!g_child) { printf("no child editor\n"); return 1; }
    printf("editor hwnd=%p\n", (void*)g_child);
    for (i = 0; i < 8; i++) {
        int n = 0;
        while (n++ < 32 && PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg); DispatchMessageA(&msg);
        }
        Sleep(40);
    }
    if (getenv("KF_CAPTURE_SCALE")) {
        click_ctl(g_child, 884, 590);
        pump_ms(&msg, 4, 40);
        click_ctl(g_child, 370, 245);
        pump_ms(&msg, 10, 50);
        SetWindowPos(parent, NULL, 40, 40, 1400, 940, SWP_NOZORDER | SWP_NOMOVE);
        pump_ms(&msg, 10, 50);
    } else if (getenv("KF_CAPTURE_PICK_MAIN")) {
        click_ctl(g_child, 884, 590);
        pump_ms(&msg, 4, 40);
        click_ctl(g_child, 600, 330);
        pump_ms(&msg, 4, 40);
        click_ctl(g_child, 400, 420);
        pump_ms(&msg, 4, 40);
        click_ctl(g_child, 870, 160);
        pump_ms(&msg, 4, 40);
        {
            Plugin* pp = (Plugin*)g_effect->object;
            printf("picked main style=%d theme=%d scale=%d\n", pp->ui_style, pp->ui_theme, pp->ui_scale);
        }
    } else if (getenv("KF_CAPTURE_PICK")) {
        click_ctl(g_child, 884, 590);
        pump_ms(&msg, 4, 40);
        click_ctl(g_child, 600, 330);
        pump_ms(&msg, 4, 40);
        click_ctl(g_child, 400, 420);
        pump_ms(&msg, 4, 40);
        {
            Plugin* pp = (Plugin*)g_effect->object;
            printf("picked style=%d theme=%d scale=%d\n", pp->ui_style, pp->ui_theme, pp->ui_scale);
        }
    } else if (getenv("KF_CAPTURE_SETTINGS")) {
        click_ctl(g_child, 884, 590);
        pump_ms(&msg, 4, 40);
    }
    GetClientRect(g_child, &rc);
    printf("client %ld x %ld\n", (long)rc.right, (long)rc.bottom);

    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = rc.right;
    bi.bmiHeader.biHeight = -rc.bottom;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    pdc = GetDC(g_child);
    mdc = CreateCompatibleDC(pdc);
    bmp = CreateDIBSection(pdc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    old = SelectObject(mdc, bmp);
    BitBlt(mdc, 0, 0, rc.right, rc.bottom, pdc, 0, 0, SRCCOPY);
    if (getenv("KF_CAPTURE_SCALE"))
        write_bmp32("assets\\ui_runtime_125.bmp", (const unsigned char*)bits, rc.right, rc.bottom);
    else if (getenv("KF_CAPTURE_PICK_MAIN"))
        write_bmp32("assets\\ui_runtime_style2_main.bmp", (const unsigned char*)bits, rc.right, rc.bottom);
    else if (getenv("KF_CAPTURE_PICK"))
        write_bmp32("assets\\ui_runtime_style2.bmp", (const unsigned char*)bits, rc.right, rc.bottom);
    else if (getenv("KF_CAPTURE_SETTINGS"))
        write_bmp32("assets\\ui_runtime_settings.bmp", (const unsigned char*)bits, rc.right, rc.bottom);
    else
        write_bmp32("assets\\ui_runtime.bmp", (const unsigned char*)bits, rc.right, rc.bottom);
    SelectObject(mdc, old);
    DeleteObject(bmp);
    DeleteDC(mdc);
    ReleaseDC(g_child, pdc);

    g_effect->dispatcher(g_effect, effEditClose, 0, 0, NULL, 0.0f);
    g_effect->dispatcher(g_effect, effClose, 0, 0, NULL, 0.0f);
    DestroyWindow(parent);
    FreeLibrary(dll);
    printf("wrote assets\\ui_runtime.bmp\n");
    return 0;
}
