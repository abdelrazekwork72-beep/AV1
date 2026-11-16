#pragma once
// Clean-room C++17 Adobe Premiere Pro MediaCore Importer ABI header.
#include "PrSDKPixelFormat.h"
#include "PrSDKTypes.h"

using imFileRef = void*;

enum imSelector : csSDK_int32 {
    imInit = 0,
    imShutdown = 1,
    imImportImage = 5,
    imQuietFile = 8,
    imCloseFile = 9,
    imAnalysis = 12,
    imDataRateAnalysis = 13,
    imGetIndFormat = 14,
    imGetMetaData = 23,
    imSetMetaData = 24,
    imImportAudio7 = 25,
    imGetIndPixelFormat = 27,
    imGetSupports7 = 28,
    imResetSequentialAudio = 29,
    imGetSequentialAudio = 30,
    imGetSupports8 = 36,
    imGetInfo8 = 37,
    imGetPrefs8 = 38,
    imGetPrefs = 38,
    imOpenFile8 = 39,
    imGetPreferredFrameSize = 45,
    imCreateAsyncImporter = 46,
    imGetSourceVideo = 47,
    imGetPeakAudio = 49,
    imGetTimeInfo8 = 52,
    imSetTimeInfo8 = 53,
    imGetAudioChannelLayout = 64,
    imSelectClipFrameDescriptor = 65,
    imGetIndColorSpace = 73,
    imGetInfo9 = 81
};

enum imReturnValue : csSDK_int32 {
    malNoError = 0,
    imNoErr = 0,
    imTooWide = 1,
    imBadFile = 2,
    imUnsupported = 3,
    imMemErr = 4,
    imOtherErr = 5,
    imNoContent = 6,
    imBadRate = 7,
    imBadCompression = 8,
    imBadCodec = 9,
    imNoTimecode = 12,
    imIterateStreams = 18,
    imBadStreamIndex = 19,
    imIterateFrameSizes = 24,
    imFrameNotFound = 28,
    imBadHeader = 29,
    imUnsupportedCompression = 30,
    imFileOpenFailed = 31,
    imFileHasNoImportableStreams = 32,
    imFileReadFailed = 33,
    imUnsupportedAudioFormat = 34,
    imDecompressionError = 36,
    imInvalidPreferences = 37,
    malUnknownError = 99,
    malSupports7 = 100,
    malSupports8 = 101,
    malSupportsPerInstancePrefs = 102,
    imDelegate = 255,
    imCancel = 300,
    imBadFormatIndex = 301,
    imIsCacheable = 400
};

struct imStdParms {
    csSDK_int32 imInterfaceVer = 8;
    csSDK_int32 reserved0 = 0;
    csSDK_int32 reserved1 = 0;
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
