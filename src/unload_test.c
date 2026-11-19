#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vst2.h"

typedef AEffect* (*VstMainFn)(audioMasterCallback);

static LRESULT CALLBACK parent_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    return DefWindowProcA(h, m, w, l);
}

static HWND make_parent(void) {
    WNDCLASSA wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = parent_proc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "KickForgeUnloadHost";
    RegisterClassA(&wc);
    return CreateWindowExA(0, "KickForgeUnloadHost", "host", WS_OVERLAPPEDWINDOW,
                           0, 0, 640, 480, NULL, NULL, wc.hInstance, NULL);
}

static void pump(int ms) {
    DWORD end = GetTickCount() + (DWORD)ms;
    MSG msg;
    int guard = 0;
    while ((int)(GetTickCount() - end) < 0 && guard++ < 500) {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        Sleep(2);
    }
}

static int cycle(const char* path, int open_editor, int destroy_parent_first) {
    HMODULE lib;
    VstMainFn mainfn;
    AEffect* e;
    HWND parent = NULL;
    int ok = 1;
    lib = LoadLibraryA(path);
    if (!lib) { printf("FAIL load %lu\n", (unsigned long)GetLastError()); return 0; }
    mainfn = (VstMainFn)GetProcAddress(lib, "VSTPluginMain");
    if (!mainfn) { printf("FAIL export\n"); FreeLibrary(lib); return 0; }
    e = mainfn(NULL);
    if (!e) { printf("FAIL instance\n"); FreeLibrary(lib); return 0; }
    e->dispatcher(e, effOpen, 0, 0, NULL, 0.0f);
    e->dispatcher(e, effSetSampleRate, 0, 0, NULL, 48000.0f);
    e->dispatcher(e, effSetBlockSize, 0, 512, NULL, 0.0f);
    e->dispatcher(e, effMainsChanged, 0, 1, NULL, 0.0f);
    if (open_editor) {
        parent = make_parent();
        if (!parent) { printf("FAIL parent\n"); ok = 0; }
        else {
            e->dispatcher(e, effEditOpen, 0, 0, parent, 0.0f);
            pump(50);
        }
    }
    if (parent && destroy_parent_first) {
        DestroyWindow(parent);
        parent = NULL;
        pump(40);
    }
    /* Deliberately do not call effEditClose/effClose: simulate a host
       unloading the DLL while its editor is still alive. */
    FreeLibrary(lib);
    pump(120);
    if (parent) DestroyWindow(parent);
    printf("cycle open=%d parent_first=%d ok=%d\n", open_editor, destroy_parent_first, ok);
    return ok;
}

static int cycle_normal(const char* path) {
    HMODULE lib;
    VstMainFn mainfn;
    AEffect* e;
    HWND parent;
    lib = LoadLibraryA(path);
    if (!lib) return 0;
    mainfn = (VstMainFn)GetProcAddress(lib, "VSTPluginMain");
    if (!mainfn) { FreeLibrary(lib); return 0; }
    e = mainfn(NULL);
    if (!e) { FreeLibrary(lib); return 0; }
    e->dispatcher(e, effOpen, 0, 0, NULL, 0.0f);
    parent = make_parent();
    e->dispatcher(e, effEditOpen, 0, 0, parent, 0.0f);
    pump(40);
    e->dispatcher(e, effEditClose, 0, 0, NULL, 0.0f);
    e->dispatcher(e, effClose, 0, 0, NULL, 0.0f);
    DestroyWindow(parent);
    FreeLibrary(lib);
    pump(50);
    printf("cycle normal OK\n");
    return 1;
}

int main(void) {
    int ok = 1;
    printf("unload test\n");
    if (!cycle("dist\\KickForge.dll", 0, 0)) ok = 0;
    if (!cycle("dist\\KickForge.dll", 1, 0)) ok = 0;
    if (!cycle("dist\\KickForge.dll", 1, 0)) ok = 0;
    if (!cycle("dist\\KickForge.dll", 1, 1)) ok = 0;
    if (!cycle_normal("dist\\KickForge.dll")) ok = 0;
    printf(ok ? "PASS unload lifecycle\n" : "FAIL unload lifecycle\n");
    return ok ? 0 : 1;
}
