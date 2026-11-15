#pragma once
// Clean-room minimal C++17 mock header for CI/testing - zero proprietary Adobe SDK code.
#include "PrSDKPixelFormat.h"
#include "PrSDKTypes.h"

using imFileRef = void*;

enum imSelector : csSDK_int32 {
    imInit = 0,
    imShutdown = 1,
    imGetInfo8 = 2,
    imOpenFile8 = 3,
    imCloseFile = 4,
    imGetIndFormat = 5,
    imGetSourceVideo = 6,
    imGetAudio = 7,
    imGetIndPixelFormat = 8,
    imImportAudio7 = 9,
    imGetPeakAudio = 10,
    imGetAudioChannelLayout = 11,
    imCreateAsyncImporter = 12,
    imQuietFile = 13,
    imGetPrefs8 = 14,
    imGetPrefs = 14
};

enum imReturnValue : csSDK_int32 {
    malNoError = 0,
    imUnsupported = -1,
    imMemErr = -2,
    imBadFile = 1001,
    imBadFormatIndex = 1002,
    imIterateStreams = 1003,
    imBadStreamIndex = 1004
};

struct imStdParms {
    csSDK_int32 imInterfaceVer = 1;
    SPBasicSuite* piSuites = nullptr;
};

struct imImportInfoRec {
    prBool canOpen = 0;
    prBool hasSetup = 0;
    prBool canSave = 0;
    prBool canDelete = 0;
    prBool canCalcSizes = 0;
    prBool noFile = 0;
    prBool avoidAudioConform = 0;
    prBool canProvidePeakAudio = 0;
    csSDK_int32 priority = 0;
    prBool isHighPriority = 0;
    prBool addToMenu = 0;
    prBool canCopy = 0;
    prBool canTrim = 0;
    prBool canCreateCustom = 0;
    prBool dontCache = 0;
    prBool supportsAsyncIO = 0;
    prBool supportsGetSourceVideo = 0;
};

struct imFileAccessRec8 {
    const char16_t* filepath = nullptr;
    csSDK_uint32 filetype = 0;
    imFileRef fileref = nullptr;
    csSDK_uint32 openMode = 0;
    prBool newFile = 0;
};

struct imFileOpenRec8 {
    imFileAccessRec8 fileinfo{};
    void* privatedata = nullptr;
    csSDK_int32 inReadWrite = 0;
    csSDK_uint64 outExtraMemoryUsage = 0;
    imFileRef fileRef = nullptr;
};

using imOpenFileRec8 = imFileOpenRec8;

struct imQuietFileRec {
    void* privatedata = nullptr;
    imFileRef fileRef = nullptr;
    prBool closeFile = 1;
    prBool releaseResources = 1;
};

struct imGetPrefsRec {
    void* privatedata = nullptr;
    void* prefs = nullptr;
    csSDK_int32 prefsLength = 0;
    prBool firstTime = 0;
    csSDK_uint32 filetype = 0;
    imFileAccessRec8 fileinfo{};
    csSDK_int32 sessionPluginID = 0;
};

struct imFileInfoRec8 {
    void* privatedata = nullptr;
    csSDK_int32 streamIdx = 0;
    csSDK_uint32 vidDuration = 0;
    csSDK_int32 vidScale = 0;
    csSDK_int32 vidSampleSize = 0;
    csSDK_int32 vidWidth = 0;
    csSDK_int32 vidHeight = 0;
    csSDK_uint32 pixelAspectNum = 1;
    csSDK_uint32 pixelAspectDen = 1;
    prBool hasVideo = 0;
    prBool hasAudio = 0;
    csSDK_uint64 audDuration = 0;
    double audSampleRate = 48000.0;
    csSDK_int32 audNumChannels = 0;
    csSDK_uint32 accessModes = 0;
    prBool highMemUsage = 0;
    prBool streamsAsComp = 0;
};

struct imFormatRec {
    csSDK_uint32 filetype = 0;
    prBool canImport = 1;
};

struct imSourceVideoRec {
    PrTime inFrame = 0;
    PPixHand outFrame = nullptr;
};

struct imImportAudioRec {
    PrTime position = 0;
    csSDK_uint32 size = 0;
    float** buffer = nullptr;
};
