/* KickForge - minimal VST2.4 ABI definitions (64-bit Windows) */
#ifndef KF_VST2_H
#define KF_VST2_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KF_VST_MAGIC 0x56737450 /* 'VstP' */

typedef struct AEffect AEffect;

typedef intptr_t (*audioMasterCallback)(AEffect*, int32_t opcode, int32_t index,
                                        intptr_t value, void* ptr, float opt);
typedef intptr_t (*AEffectDispatcherProc)(AEffect*, int32_t opcode, int32_t index,
                                          intptr_t value, void* ptr, float opt);
typedef void (*AEffectProcessProc)(AEffect*, float** inputs, float** outputs, int32_t sampleframes);
typedef void (*AEffectProcessDoubleProc)(AEffect*, double** inputs, double** outputs, int32_t sampleframes);
typedef void (*AEffectSetParameterProc)(AEffect*, int32_t index, float parameter);
typedef float (*AEffectGetParameterProc)(AEffect*, int32_t index);

struct AEffect {
    int32_t magic;
    AEffectDispatcherProc dispatcher;
    AEffectProcessProc process;
    AEffectSetParameterProc setParameter;
    AEffectGetParameterProc getParameter;
    int32_t numPrograms;
    int32_t numParams;
    int32_t numInputs;
    int32_t numOutputs;
    int32_t flags;
    intptr_t ptr1;
    intptr_t ptr2;
    int32_t initialDelay;
    int32_t realQualities;
    int32_t offQualities;
    float ioRatio;
    void* object;
    void* user;
    int32_t uniqueID;
    int32_t version;
    AEffectProcessProc processReplacing;
    AEffectProcessDoubleProc processDoubleReplacing;
    char future[56];
};

typedef struct ERect {
    int16_t top;
    int16_t left;
    int16_t bottom;
    int16_t right;
} ERect;

/* Plugin -> host opcodes */
enum {
    audioMasterAutomate = 0,
    audioMasterVersion = 1,
    audioMasterCurrentId = 2,
    audioMasterIdle = 3,
    audioMasterPinConnected = 4,
    audioMasterWantMidi = 6,
    audioMasterGetTime = 7,
    audioMasterProcessEvents = 8,
    audioMasterSetTime = 9,
    audioMasterTempoAt = 10,
    audioMasterGetNumAutomatableParameters = 11,
    audioMasterGetParameterQuantization = 12,
    audioMasterIOChanged = 13,
    audioMasterNeedIdle = 14,
    audioMasterSizeWindow = 15,
    audioMasterGetSampleRate = 16,
    audioMasterGetBlockSize = 17,
    audioMasterGetInputLatency = 18,
    audioMasterGetOutputLatency = 19,
    audioMasterGetPreviousPlug = 20,
    audioMasterGetNextPlug = 21,
    audioMasterWillReplaceOrAccumulate = 22,
    audioMasterGetCurrentProcessLevel = 23,
    audioMasterGetAutomationState = 24,
    audioMasterOfflineStart = 25,
    audioMasterOfflineRead = 26,
    audioMasterOfflineWrite = 27,
    audioMasterOfflineGetCurrentPass = 28,
    audioMasterOfflineGetCurrentMetaPass = 29,
    audioMasterSetOutputSampleRate = 30,
    audioMasterGetOutputSpeakerArrangement = 31,
    audioMasterGetLanguage = 32,
    audioMasterOpenWindow = 33,
    audioMasterCloseWindow = 34,
    audioMasterGetDirectory = 35,
    audioMasterUpdateDisplay = 36,
    audioMasterBeginEdit = 37,
    audioMasterEndEdit = 38,
    audioMasterOpenFileSelector = 39,
    audioMasterCloseFileSelector = 40,
    audioMasterEditFile = 41,
    audioMasterGetChunkFile = 42,
    audioMasterGetInputSpeakerArrangement = 43
};

/* Host -> plugin opcodes */
enum {
    effOpen = 0,
    effClose = 1,
    effSetProgram = 2,
    effGetProgram = 3,
    effSetProgramName = 4,
    effGetProgramName = 5,
    effGetParamLabel = 6,
    effGetParamDisplay = 7,
    effGetParamName = 8,
    effGetVu = 9,
    effSetSampleRate = 10,
    effSetBlockSize = 11,
    effMainsChanged = 12,
    effEditGetRect = 13,
    effEditOpen = 14,
    effEditClose = 15,
    effEditDraw = 16,
    effEditMouse = 17,
    effEditKey = 18,
    effEditIdle = 19,
    effEditTop = 20,
    effEditSleep = 21,
    effIdentify = 22,
    effGetChunk = 23,
    effSetChunk = 24,
    effProcessEvents = 25,
    effCanBeAutomated = 26,
    effString2Parameter = 27,
    effGetNumProgramCategories = 28,
    effGetProgramNameIndexed = 29,
    effCopyProgram = 30,
    effConnectInput = 31,
    effConnectOutput = 32,
    effGetInputText = 33,
    effGetOutputText = 34,
    effGetPlugCategory = 35,
    effGetCurrentPosition = 36,
    effGetDestinationBuffer = 37,
    effOfflineNotify = 38,
    effOfflinePrepare = 39,
    effOfflineRun = 40,
    effProcessVarIo = 41,
    effSetSpeakerArrangement = 42,
    effSetBlockSizeAndSampleRate = 43,
    effSetBypass = 44,
    effGetEffectName = 45,
    effGetErrorText = 46,
    effGetVendorString = 47,
    effGetProductString = 48,
    effGetVendorVersion = 49,
    effVendorSpecific = 50,
    effCanDo = 51,
    effGetTailSize = 52,
    effIdle = 53,
    effGetIcon = 54,
    effSetViewPosition = 55,
    effGetParameterProperties = 56,
    effKeysRequired = 57,
    effGetVstVersion = 58
};

enum {
    effFlagsHasEditor = 1 << 0,
    effFlagsCanReplacing = 1 << 4,
    effFlagsProgramChunks = 1 << 5,
    effFlagsIsSynth = 1 << 8,
    effFlagsNoSoundInStop = 1 << 9,
    effFlagsHasVu = 1 << 10,
    effFlagsCanMono = 1 << 11,
    effFlagsCanDoubleReplacing = 1 << 12
};

enum {
    kPlugCategUnknown = 0,
    kPlugCategEffect = 1,
    kPlugCategSynth = 2,
    kPlugCategAnalysis = 3,
    kPlugCategMastering = 4
};

#ifdef __cplusplus
}
#endif

#endif
