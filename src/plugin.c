#include "kf.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

static float g_zero[KF_MAX_BLOCK];
HINSTANCE kf_dll_instance = NULL;

static intptr_t kf_dispatcher(AEffect* e, int32_t opcode, int32_t index,
                              intptr_t value, void* ptr, float opt);
static void kf_process_replacing(AEffect* e, float** inputs, float** outputs, int32_t n);
static void kf_set_parameter(AEffect* e, int32_t index, float value);
static float kf_get_parameter(AEffect* e, int32_t index);

static void kf_copy_str(char* dst, const char* src, int max) {
    int i = 0;
    if (!dst || max <= 0) return;
    while (src && src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static void kf_plugin_set_edit_size(Plugin* p) {
    int pct;
    if (!p) return;
    pct = (p->ui_scale == 0) ? 100 : (p->ui_scale == 1 ? 125 : 150);
    p->edit_rect.left = 0;
    p->edit_rect.top = 0;
    p->edit_rect.right = KF_UI_W * pct / 100;
    p->edit_rect.bottom = KF_UI_H * pct / 100;
}

static void kf_settings_path(char* out, int cap) {
    const char* override = getenv("KF_SETTINGS_FILE");
    const char* app = getenv("APPDATA");
    char dir[512];
    if (override && override[0]) {
        strncpy(out, override, (size_t)cap - 1);
        out[cap - 1] = 0;
        return;
    }
    if (app && app[0]) {
        sprintf(dir, "%s\\KickForge", app);
        CreateDirectoryA(dir, NULL);
        sprintf(out, "%s\\settings.ini", dir);
    } else {
        char dll[512];
        char* slash;
        dll[0] = 0;
        GetModuleFileNameA(kf_dll_instance, dll, sizeof(dll));
        slash = strrchr(dll, '\\');
        if (slash) slash[1] = 0;
        sprintf(out, "%sKickForge.ini", dll);
    }
    out[cap - 1] = 0;
}

static void kf_settings_dll_path(char* out, int cap) {
    char dll[512];
    char* slash;
    dll[0] = 0;
    GetModuleFileNameA(kf_dll_instance, dll, sizeof(dll));
    slash = strrchr(dll, '\\');
    if (slash) slash[1] = 0;
    sprintf(out, "%sKickForge.ini", dll);
    out[cap - 1] = 0;
}

static int kf_settings_read(const char* path, Plugin* p) {
    FILE* f;
    char line[256];
    if (!path || !path[0] || !p) return 0;
    f = fopen(path, "r");
    if (!f) return 0;
    while (fgets(line, sizeof(line), f)) {
        int v;
        if (sscanf(line, "scale=%d", &v) == 1) {
            p->ui_scale = (v < 0) ? 0 : (v > 2 ? 2 : v);
        } else if (sscanf(line, "style=%d", &v) == 1) {
            p->ui_style = (v < 0) ? 0 : (v > 1 ? 1 : v);
        } else if (sscanf(line, "theme=%d", &v) == 1) {
            p->ui_theme = (v < 0) ? 0 : (v > 3 ? 3 : v);
        }
    }
    fclose(f);
    return 1;
}

int kf_settings_load(Plugin* p) {
    char path[512];
    char alt[512];
    const char* override = getenv("KF_SETTINGS_FILE");
    if (!p) return 0;
    kf_settings_path(path, (int)sizeof(path));
    if (kf_settings_read(path, p)) return 1;
    if (override && override[0]) return 0;
    kf_settings_dll_path(alt, (int)sizeof(alt));
    if (strcmp(alt, path) != 0 && kf_settings_read(alt, p)) return 1;
    if (strcmp(path, "KickForge.ini") != 0 && kf_settings_read("KickForge.ini", p)) return 1;
    return 0;
}

static int kf_settings_write(const Plugin* p, const char* path) {
    FILE* f;
    char tmp[512];
    int ok;
    if (!path || !path[0]) return 0;
    sprintf(tmp, "%s.tmp", path);
    f = fopen(tmp, "w");
    if (!f) return 0;
    fprintf(f, "# KickForge UI settings\n");
    fprintf(f, "scale=%d\n", p->ui_scale);
    fprintf(f, "style=%d\n", p->ui_style);
    fprintf(f, "theme=%d\n", p->ui_theme);
    ok = (fclose(f) == 0);
    if (!ok) return 0;
    if (!MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileA(path);
        MoveFileA(tmp, path);
    }
    return 1;
}

int kf_settings_save(const Plugin* p) {
    char path[512];
    char alt[512];
    const char* override = getenv("KF_SETTINGS_FILE");
    if (!p) return 0;
    kf_settings_path(path, (int)sizeof(path));
    if (kf_settings_write(p, path)) return 1;
    if (override && override[0]) return 0;
    kf_settings_dll_path(alt, (int)sizeof(alt));
    if (strcmp(alt, path) != 0 && kf_settings_write(p, alt)) return 1;
    if (strcmp(path, "KickForge.ini") != 0 && kf_settings_write(p, "KickForge.ini")) return 1;
    return 0;
}

void kf_plugin_notify(Plugin* p, int opcode, int index, intptr_t value, void* ptr, float opt) {
    if (p && p->host && p->effect.magic == KF_VST_MAGIC) {
        p->host(&p->effect, opcode, index, value, ptr, opt);
    }
}

void kf_plugin_begin_edit(Plugin* p, int idx) {
    kf_plugin_notify(p, audioMasterBeginEdit, idx, 0, NULL, 0.0f);
}

void kf_plugin_end_edit(Plugin* p, int idx) {
    kf_plugin_notify(p, audioMasterEndEdit, idx, 0, NULL, 0.0f);
}

void kf_plugin_init(Plugin* p, audioMasterCallback host) {
    int i;
    memset(p, 0, sizeof(*p));
    p->host = host;
    p->sr = 44100.0f;
    p->block_size = 512;
    p->current_preset = 0;
    p->preset_dirty = 0;
    p->rng = 0x9E3779B9u;
    for (i = 0; i < NPARAMS; i++) p->params[i] = kf_param_default(i);
    kf_snapshot(p->params, &p->snap);
    kf_dsp_init(&p->dsp, p->sr);
    kf_dsp_prime(&p->dsp, &p->snap);
    p->peak_l = p->peak_r = p->rms_l = p->rms_r = 0.0f;
    p->clip_led = 0.0f;
    p->scope_pos = 0;
    p->edit_rect.left = 0;
    p->edit_rect.top = 0;
    p->edit_rect.right = KF_UI_W;
    p->edit_rect.bottom = KF_UI_H;
    p->ui_scale = 0;
    p->ui_theme = 0;
    p->ui_style = 0;
    p->ui_bg = 0;
    kf_settings_load(p);
    kf_plugin_set_edit_size(p);

    memset(&p->effect, 0, sizeof(p->effect));
    p->effect.magic = KF_VST_MAGIC;
    p->effect.dispatcher = kf_dispatcher;
    p->effect.process = kf_process_replacing;
    p->effect.setParameter = kf_set_parameter;
    p->effect.getParameter = kf_get_parameter;
    p->effect.numPrograms = KF_NUM_PRESETS;
    p->effect.numParams = NPARAMS;
    p->effect.numInputs = 2;
    p->effect.numOutputs = 2;
    p->effect.flags = effFlagsHasEditor | effFlagsCanReplacing | effFlagsProgramChunks;
    p->effect.initialDelay = 14;
    p->effect.realQualities = 0;
    p->effect.offQualities = 0;
    p->effect.ioRatio = 1.0f;
    p->effect.object = p;
    p->effect.user = NULL;
    p->effect.uniqueID = 0x4B663031; /* Kf01 */
    p->effect.version = KF_VERSION;
    p->effect.processReplacing = kf_process_replacing;
    p->effect.processDoubleReplacing = NULL;
}

void kf_plugin_destroy(Plugin* p) {
    if (!p) return;
    if (p->editor) {
        kf_editor_destroy(p->editor);
        p->editor = NULL;
    }
    free(p);
}

void kf_plugin_set_sr(Plugin* p, float sr) {
    if (!p) return;
    if (sr < 8000.0f) sr = 8000.0f;
    if (sr > 384000.0f) sr = 384000.0f;
    p->sr = sr;
    kf_dsp_set_sr(&p->dsp, sr);
    p->peak_l = p->peak_r = p->rms_l = p->rms_r = 0.0f;
}

void kf_plugin_set_block(Plugin* p, int n) {
    if (!p) return;
    if (n < 1) n = 1;
    p->block_size = n;
}

void kf_plugin_reset(Plugin* p) {
    int i;
    if (!p) return;
    kf_dsp_reset(&p->dsp);
    p->peak_l = p->peak_r = p->rms_l = p->rms_r = 0.0f;
    p->clip_led = 0.0f;
    for (i = 0; i < KF_SCOPE_SIZE; i++) p->scope[i] = 0.0f;
    p->scope_pos = 0;
}

void kf_plugin_set_param(Plugin* p, int idx, float norm) {
    if (!p || idx < 0 || idx >= NPARAMS) return;
    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;
    p->params[idx] = norm;
    p->preset_dirty = 1;
    kf_snapshot(p->params, &p->snap);
}

void kf_plugin_set_param_ui(Plugin* p, int idx, float norm) {
    if (!p || idx < 0 || idx >= NPARAMS) return;
    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;
    kf_plugin_begin_edit(p, idx);
    p->params[idx] = norm;
    p->preset_dirty = 1;
    kf_snapshot(p->params, &p->snap);
    kf_plugin_notify(p, audioMasterAutomate, idx, 0, NULL, norm);
    kf_plugin_end_edit(p, idx);
}

float kf_plugin_get_param(Plugin* p, int idx) {
    if (!p || idx < 0 || idx >= NPARAMS) return 0.0f;
    return p->params[idx];
}

void kf_plugin_load_preset(Plugin* p, int idx) {
    int i;
    if (!p) return;
    if (idx < 0 || idx >= KF_NUM_PRESETS) return;
    for (i = 0; i < NPARAMS; i++) {
        p->params[i] = kf_param_real_to_norm(i, kf_presets[idx].values[i]);
    }
    p->current_preset = idx;
    p->preset_dirty = 0;
    kf_snapshot(p->params, &p->snap);
    kf_dsp_reset(&p->dsp);
    kf_dsp_prime(&p->dsp, &p->snap);
}

void kf_plugin_process(Plugin* p, float** inputs, float** outputs, int n) {
    float* inL;
    float* inR;
    float* outL;
    float* outR;
    int off;
    int i;
    float pl = 0.0f, pr = 0.0f;
    double rl = 0.0, rr = 0.0;

    if (!p || n <= 0) return;
    if (!outputs || !outputs[0]) return;
    outL = outputs[0];
    outR = outputs[1] ? outputs[1] : outputs[0];
    inL = (inputs && inputs[0]) ? inputs[0] : g_zero;
    inR = (inputs && inputs[1]) ? inputs[1] : inL;

    if (p->suspended) {
        memset(outL, 0, sizeof(float) * (size_t)n);
        if (outR != outL) memset(outR, 0, sizeof(float) * (size_t)n);
        return;
    }

    kf_snapshot(p->params, &p->snap);

    if (p->host_bypass || p->snap.bypass) {
        for (i = 0; i < n; i++) {
            outL[i] = inL[i];
            outR[i] = inR[i];
        }
    } else {
        for (off = 0; off < n; off += KF_MAX_BLOCK) {
            int m = n - off;
            if (m > KF_MAX_BLOCK) m = KF_MAX_BLOCK;
            kf_dsp_process(&p->dsp, inL + off, inR + off, outL + off, outR + off, m, &p->snap);
        }
    }

    for (i = 0; i < n; i++) {
        float al = (float)fabs((double)outL[i]);
        float ar = (float)fabs((double)outR[i]);
        if (al > pl) pl = al;
        if (ar > pr) pr = ar;
        rl += (double)outL[i] * (double)outL[i];
        rr += (double)outR[i] * (double)outR[i];
        p->scope[p->scope_pos & (KF_SCOPE_SIZE - 1)] = 0.5f * (outL[i] + outR[i]);
        p->scope_pos = (p->scope_pos + 1) & (KF_SCOPE_SIZE - 1);
    }
    rl = sqrt(rl / (double)n);
    rr = sqrt(rr / (double)n);

    p->peak_l = (pl > p->peak_l * 0.82f) ? pl : p->peak_l * 0.82f;
    p->peak_r = (pr > p->peak_r * 0.82f) ? pr : p->peak_r * 0.82f;
    p->rms_l = (float)(p->rms_l * 0.82 + rl * 0.18);
    p->rms_r = (float)(p->rms_r * 0.82 + rr * 0.18);
    if (pl >= (float)p->snap.ceiling * 0.985f || pr >= (float)p->snap.ceiling * 0.985f) {
        p->clip_led = 1.0f;
    }
}

/* ------------------------------------------------------------------ */
/* State chunk                                                         */
/* ------------------------------------------------------------------ */
#define KF_CHUNK_MAGIC 0x4843464Bu /* 'KFCH' */

static unsigned int kf_crc(const unsigned char* data, int n) {
    unsigned int h = 2166136261u;
    int i;
    for (i = 0; i < n; i++) {
        h ^= data[i];
        h *= 16777619u;
    }
    return h;
}

int kf_plugin_make_chunk(Plugin* p) {
    unsigned char* b;
    int pos = 0;
    int i;
    unsigned int magic = KF_CHUNK_MAGIC;
    unsigned int ver = 3;
    int npar = NPARAMS;
    int preset;
    int dirty;
    unsigned int crc;
    if (!p) return 0;
    b = p->chunk;
    memcpy(b + pos, &magic, 4); pos += 4;
    memcpy(b + pos, &ver, 4); pos += 4;
    memcpy(b + pos, &npar, 4); pos += 4;
    preset = p->current_preset;
    dirty = p->preset_dirty;
    memcpy(b + pos, &preset, 4); pos += 4;
    memcpy(b + pos, &dirty, 4); pos += 4;
    for (i = 0; i < NPARAMS; i++) {
        memcpy(b + pos, &p->params[i], 4); pos += 4;
    }
    memcpy(b + pos, &p->ui_scale, 4); pos += 4;
    memcpy(b + pos, &p->ui_theme, 4); pos += 4;
    memcpy(b + pos, &p->ui_style, 4); pos += 4;
    memcpy(b + pos, &p->ui_bg, 4); pos += 4;
    crc = kf_crc(b, pos);
    memcpy(b + pos, &crc, 4); pos += 4;
    p->chunk_size = pos;
    return pos;
}

int kf_plugin_read_chunk(Plugin* p, const void* data, int size) {
    const unsigned char* b = (const unsigned char*)data;
    int pos = 0;
    unsigned int magic, ver, crc, want;
    int npar, preset, dirty;
    int i;
    if (!p || !data) return 0;
    if (size < 20 + NPARAMS * 4) return 0;
    memcpy(&magic, b + pos, 4); pos += 4;
    memcpy(&ver, b + pos, 4); pos += 4;
    memcpy(&npar, b + pos, 4); pos += 4;
    memcpy(&preset, b + pos, 4); pos += 4;
    memcpy(&dirty, b + pos, 4); pos += 4;
    if (magic != KF_CHUNK_MAGIC || ver < 1 || ver > 3 || npar != NPARAMS) return 0;
    if (size < pos + NPARAMS * 4 + (ver >= 3 ? 16 : (ver >= 2 ? 12 : 0)) + 4) return 0;
    for (i = 0; i < NPARAMS; i++) {
        float v;
        memcpy(&v, b + pos, 4); pos += 4;
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        p->params[i] = v;
    }
    if (ver >= 2) {
        int sc, th, bg, st = 0;
        memcpy(&sc, b + pos, 4); pos += 4;
        memcpy(&th, b + pos, 4); pos += 4;
        if (ver >= 3) {
            memcpy(&st, b + pos, 4); pos += 4;
        }
        memcpy(&bg, b + pos, 4); pos += 4;
        (void)bg;
        p->ui_scale = (sc < 0) ? 0 : (sc > 2 ? 2 : sc);
        p->ui_theme = (th < 0) ? 0 : (th > 3 ? 3 : th);
        p->ui_style = (st < 0) ? 0 : (st > 1 ? 1 : st);
        p->ui_bg = 0;
    }
    memcpy(&crc, b + pos, 4); pos += 4;
    want = kf_crc(b, pos - 4);
    if (crc != want) return 0;
    p->current_preset = preset;
    p->preset_dirty = dirty;
    kf_plugin_set_edit_size(p);
    kf_snapshot(p->params, &p->snap);
    kf_dsp_reset(&p->dsp);
    kf_dsp_prime(&p->dsp, &p->snap);
    if (p->editor) kf_editor_queue_settings(p->editor);
    return 1;
}

/* ------------------------------------------------------------------ */
/* VST entry points                                                    */
/* ------------------------------------------------------------------ */
static void kf_set_parameter(AEffect* e, int32_t index, float value) {
    if (!e || !e->object) return;
    kf_plugin_set_param((Plugin*)e->object, (int)index, value);
}

static float kf_get_parameter(AEffect* e, int32_t index) {
    if (!e || !e->object) return 0.0f;
    return kf_plugin_get_param((Plugin*)e->object, (int)index);
}

static void kf_process_replacing(AEffect* e, float** inputs, float** outputs, int32_t n) {
    if (!e || !e->object) return;
    kf_plugin_process((Plugin*)e->object, inputs, outputs, (int)n);
}

static intptr_t kf_dispatcher(AEffect* e, int32_t opcode, int32_t index,
                              intptr_t value, void* ptr, float opt) {
    Plugin* p;
    if (!e) return 0;
    p = (Plugin*)e->object;
    if (!p) return 0;

    switch (opcode) {
    case effOpen:
        return 0;
    case effClose:
        kf_plugin_destroy(p);
        return 0;
    case effSetProgram: {
        int prog = (int)value;
        if (prog < 0 || prog >= KF_NUM_PRESETS) prog = (int)index;
        kf_plugin_load_preset(p, prog);
        return 0;
    }
    case effGetProgram:
        return p->current_preset;
    case effSetProgramName:
        return 0;
    case effGetProgramName:
        kf_copy_str((char*)ptr, kf_preset_name(p->current_preset), 64);
        return 0;
    case effGetProgramNameIndexed: {
        int prog = (int)index;
        if (prog < 0 || prog >= KF_NUM_PRESETS) prog = (int)value;
        if (prog < 0 || prog >= KF_NUM_PRESETS) prog = 0;
        kf_copy_str((char*)ptr, kf_preset_name(prog), 64);
        return 1;
    }
    case effGetParamLabel:
        if (index >= 0 && index < NPARAMS) kf_param_label(index, (char*)ptr, 64);
        return 0;
    case effGetParamDisplay:
        if (index >= 0 && index < NPARAMS) kf_param_format(index, p->params[index], (char*)ptr, 64);
        return 0;
    case effGetParamName:
        if (index >= 0 && index < NPARAMS) kf_copy_str((char*)ptr, kf_param_defs[index].name, 64);
        return 0;
    case effGetVu:
        return 0;
    case effSetSampleRate:
        kf_plugin_set_sr(p, opt);
        return 0;
    case effSetBlockSize:
        kf_plugin_set_block(p, (int)value);
        return 0;
    case effMainsChanged:
        if (value == 0) {
            p->suspended = 1;
            kf_plugin_reset(p);
        } else {
            p->suspended = 0;
            kf_plugin_reset(p);
        }
        return 0;
    case effEditGetRect:
        if (ptr) *(ERect**)ptr = &p->edit_rect;
        return 1;
    case effEditOpen: {
        HWND parent;
        if (p->editor) return 1;
        parent = (HWND)(ptr ? ptr : (void*)value);
        if (parent && !IsWindow(parent)) return 0;
        p->editor = kf_editor_create(parent, p);
        return p->editor ? 1 : 0;
    }
    case effEditClose:
        if (p->editor) {
            kf_editor_destroy(p->editor);
            p->editor = NULL;
        }
        return 0;
    case effEditIdle:
        if (p->editor) kf_editor_tick(p->editor);
        return 0;
    case effEditTop:
    case effEditSleep:
    case effEditDraw:
    case effEditMouse:
    case effEditKey:
        return 0;
    case effGetChunk:
        if (ptr) {
            int sz = kf_plugin_make_chunk(p);
            *(void**)ptr = p->chunk;
            return sz;
        }
        return 0;
    case effSetChunk:
        if (!ptr || (int)value <= 0) return 0;
        return kf_plugin_read_chunk(p, ptr, (int)value);
    case effProcessEvents:
        return 0;
    case effCanBeAutomated:
        return 1;
    case effString2Parameter:
        return 0;
    case effGetNumProgramCategories:
        return 0;
    case effCopyProgram:
        return 0;
    case effConnectInput:
    case effConnectOutput:
        return 1;
    case effGetInputText:
        kf_copy_str((char*)ptr, "Input", 64);
        return 0;
    case effGetOutputText:
        kf_copy_str((char*)ptr, "Output", 64);
        return 0;
    case effGetPlugCategory:
        return kPlugCategEffect;
    case effGetCurrentPosition:
    case effGetDestinationBuffer:
    case effOfflineNotify:
    case effOfflinePrepare:
    case effOfflineRun:
    case effProcessVarIo:
    case effSetSpeakerArrangement:
        return 0;
    case effSetBlockSizeAndSampleRate:
        kf_plugin_set_block(p, (int)value);
        kf_plugin_set_sr(p, opt);
        return 0;
    case effSetBypass:
        p->host_bypass = value ? 1 : 0;
        return 0;
    case effGetEffectName:
        kf_copy_str((char*)ptr, "KickForge", 64);
        return 1;
    case effGetErrorText:
        return 0;
    case effGetVendorString:
        kf_copy_str((char*)ptr, "KickForge Audio Lab", 64);
        return 1;
    case effGetProductString:
        kf_copy_str((char*)ptr, "KickForge", 64);
        return 1;
    case effGetVendorVersion:
        return KF_VERSION;
    case effVendorSpecific:
        return 0;
    case effCanDo:
        return 0;
    case effGetTailSize:
        return 0;
    case effIdle:
        return 0;
    case effGetIcon:
        return 0;
    case effSetViewPosition:
        return 0;
    case effGetParameterProperties:
        return 0;
    case effKeysRequired:
        return 0;
    case effGetVstVersion:
        return 2400;
    default:
        return 0;
    }
}

__declspec(dllexport) AEffect* VSTPluginMain(audioMasterCallback host) {
    Plugin* p = (Plugin*)calloc(1, sizeof(Plugin));
    if (!p) return NULL;
    kf_plugin_init(p, host);
    return &p->effect;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r) {
    (void)r;
    if (reason == DLL_PROCESS_ATTACH) {
        kf_dll_instance = h;
        DisableThreadLibraryCalls(h);
    } else if (reason == DLL_PROCESS_DETACH) {
        kf_editor_detach_all();
    }
    return TRUE;
}
