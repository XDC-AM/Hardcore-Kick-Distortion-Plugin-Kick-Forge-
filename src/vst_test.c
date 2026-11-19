#include "kf.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static intptr_t host_cb(AEffect* e, int32_t op, int32_t idx, intptr_t val, void* ptr, float opt) {
    (void)e; (void)op; (void)idx; (void)val; (void)ptr; (void)opt;
    return 0;
}

static int finite_buf(const float* b, int n) {
    int i;
    for (i = 0; i < n; i++) {
        if (b[i] != b[i]) return 0;
        if (b[i] > 1.0e6f || b[i] < -1.0e6f) return 0;
    }
    return 1;
}

int main(void) {
    HMODULE dll;
    AEffect* eff;
    intptr_t (*mainproc)(audioMasterCallback);
    intptr_t rc;
    int i, b, prog, os;
    float inL[512], inR[512], outL[512], outR[512];
    float* ins[2];
    float* outs[2];
    float max_peak = 0.0f;
    ERect* rect = NULL;
    char name[128];
    char disp[128];
    int chunk_size;
    void* chunk_ptr = NULL;
    unsigned char chunk_copy[4096];
    float drive_before, drive_after;
    HWND parent;

    printf("KickForge host validation\n");
    dll = LoadLibraryA("dist\\KickForge.dll");
    if (!dll) { printf("FAIL LoadLibrary %lu\n", (unsigned long)GetLastError()); return 1; }
    printf("OK  LoadLibrary\n");
    mainproc = (intptr_t (*)(audioMasterCallback))GetProcAddress(dll, "VSTPluginMain");
    if (!mainproc) { printf("FAIL GetProcAddress(VSTPluginMain)\n"); FreeLibrary(dll); return 1; }
    printf("OK  VSTPluginMain export\n");

    eff = (AEffect*)mainproc(host_cb);
    if (!eff) { printf("FAIL plugin main returned null\n"); FreeLibrary(dll); return 1; }
    printf("OK  AEffect @ %p magic=%08lX version=%ld\n", (void*)eff,
           (unsigned long)eff->magic, (long)eff->version);
    if (eff->magic != KF_VST_MAGIC) { printf("FAIL magic\n"); return 1; }

    rc = eff->dispatcher(eff, effOpen, 0, 0, NULL, 0.0f);
    printf("OK  effOpen rc=%ld\n", (long)rc);
    eff->dispatcher(eff, effSetSampleRate, 0, 0, NULL, 48000.0f);
    eff->dispatcher(eff, effSetBlockSize, 0, 512, NULL, 0.0f);
    eff->dispatcher(eff, effMainsChanged, 0, 1, NULL, 0.0f);
    printf("OK  sample rate 48k, block 512, mains on\n");

    memset(name, 0, sizeof(name));
    eff->dispatcher(eff, effGetEffectName, 0, 0, name, 0.0f);
    printf("OK  effect name: \"%s\"  params=%d programs=%d flags=%ld\n",
           name, (int)eff->numParams, (int)eff->numPrograms, (long)eff->flags);

    memset(disp, 0, sizeof(disp));
    eff->dispatcher(eff, effGetParamDisplay, P_DRIVE, 0, disp, 0.0f);
    printf("OK  P_DRIVE display: \"%s\" value=%.4f\n", disp, (double)eff->getParameter(eff, P_DRIVE));

    printf("OK  VST version: %ld\n", (long)eff->dispatcher(eff, effGetVstVersion, 0, 0, NULL, 0.0f));

    ins[0] = inL; ins[1] = inR; outs[0] = outL; outs[1] = outR;
    for (b = 0; b < 8; b++) {
        for (i = 0; i < 512; i++) {
            double t = (double)(b * 512 + i) / 48000.0;
            double env = exp(-t * 14.0);
            double x = sin(2.0 * 3.141592653589793 * 55.0 * t) * env;
            inL[i] = (float)(x * 0.8);
            inR[i] = inL[i];
        }
        eff->processReplacing(eff, ins, outs, 512);
        if (!finite_buf(outL, 512) || !finite_buf(outR, 512)) {
            printf("FAIL non-finite output at block %d\n", b);
            return 1;
        }
        for (i = 0; i < 512; i++) {
            float al = (float)fabs((double)outL[i]);
            float ar = (float)fabs((double)outR[i]);
            if (al > max_peak) max_peak = al;
            if (ar > max_peak) max_peak = ar;
        }
    }
    printf("OK  8 blocks processed, max peak L/R=%.4f\n", (double)max_peak);

    eff->dispatcher(eff, effSetProgram, 0, 1, NULL, 0.0f);
    printf("OK  program 1 loaded, current=%ld name=", (long)eff->dispatcher(eff, effGetProgram, 0, 0, NULL, 0.0f));
    memset(name, 0, sizeof(name));
    eff->dispatcher(eff, effGetProgramName, 0, 0, name, 0.0f);
    printf("\"%s\"\n", name);

    drive_before = eff->getParameter(eff, P_DRIVE);
    eff->setParameter(eff, P_DRIVE, 0.9f);
    chunk_size = (int)eff->dispatcher(eff, effGetChunk, 0, 0, &chunk_ptr, 0.0f);
    printf("OK  getChunk size=%d ptr=%p\n", chunk_size, chunk_ptr);
    if (chunk_size <= 0 || chunk_size > 4096 || !chunk_ptr) { printf("FAIL chunk\n"); return 1; }
    memcpy(chunk_copy, chunk_ptr, chunk_size);
    eff->dispatcher(eff, effSetProgram, 0, 0, NULL, 0.0f);
    printf("OK  chunk restore rc=%ld\n", (long)eff->dispatcher(eff, effSetChunk, 0, chunk_size, chunk_copy, 0.0f));
    drive_after = eff->getParameter(eff, P_DRIVE);
    printf("OK  drive before=%.4f after restore=%.4f (expected 0.9)\n",
           (double)drive_before, (double)drive_after);

    printf("OK  preset/oversampling sweep\n");
    for (prog = 0; prog < 10; prog++) {
        for (os = 0; os < 4; os++) {
            float peak = 0.0f;
            eff->dispatcher(eff, effSetProgram, 0, prog, NULL, 0.0f);
            eff->setParameter(eff, P_OS, (float)os / 3.0f);
            memset(inL, 0, sizeof(inL));
            memset(inR, 0, sizeof(inR));
            inL[0] = 0.85f;
            inR[0] = 0.85f;
            for (b = 0; b < 4; b++) {
                eff->processReplacing(eff, ins, outs, 512);
                if (!finite_buf(outL, 512) || !finite_buf(outR, 512)) {
                    printf("FAIL non-finite preset %d os %d block %d\n", prog, os, b);
                    return 1;
                }
                for (i = 0; i < 512; i++) {
                    float al = (float)fabs((double)outL[i]);
                    float ar = (float)fabs((double)outR[i]);
                    if (al > peak) peak = al;
                    if (ar > peak) peak = ar;
                }
                memset(inL, 0, sizeof(inL));
                memset(inR, 0, sizeof(inR));
            }
            if (peak < 1.0e-5f) {
                printf("FAIL silent preset %d os %d\n", prog, os);
                return 1;
            }
        }
    }
    printf("OK  10 presets x 4 oversampling modes finite and non-silent\n");

    printf("OK  edit rect: ");
    eff->dispatcher(eff, effEditGetRect, 0, 0, &rect, 0.0f);
    if (rect) printf("%d x %d\n", (int)(rect->right - rect->left), (int)(rect->bottom - rect->top));
    else printf("(null)\n");

    parent = CreateWindowExA(0, "STATIC", "KickForge Host", WS_OVERLAPPEDWINDOW,
                             0, 0, 1200, 760, NULL, NULL, GetModuleHandleA(NULL), NULL);
    if (parent) {
        printf("OK  host parent window\n");
        rc = eff->dispatcher(eff, effEditOpen, 0, (intptr_t)parent, NULL, 0.0f);
        printf("OK  effEditOpen rc=%ld\n", (long)rc);
        eff->dispatcher(eff, effEditIdle, 0, 0, NULL, 0.0f);
        printf("OK  effEditIdle\n");
        Sleep(120);
        eff->dispatcher(eff, effEditClose, 0, 0, NULL, 0.0f);
        printf("OK  effEditClose\n");
        DestroyWindow(parent);
    } else {
        printf("WARN parent window creation failed\n");
    }

    rc = eff->dispatcher(eff, effClose, 0, 0, NULL, 0.0f);
    printf("OK  effClose rc=%ld\n", (long)rc);
    FreeLibrary(dll);
    printf("PASS KickForge VST2 validation\n");
    return 0;
}
