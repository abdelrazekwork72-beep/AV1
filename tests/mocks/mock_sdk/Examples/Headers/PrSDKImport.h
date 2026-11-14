#pragma once
// Clean-room minimal C++17 mock header for CI/testing - zero proprietary Adobe SDK code.
#include "PrSDKPixelFormat.h"
#include "PrSDKTypes.h"

enum imSelector : csSDK_int32 {
    imInit = 0,
    imShutdown = 1,
    imGetInfo8 = 2,
    imOpenFile8 = 3,
    imCloseFile = 4,
    imGetIndFormat = 5,
    imGetSourceVideo = 6,
    imGetAudio = 7
};

enum imReturnValue : csSDK_int32 {
    malNoError = 0,
    imUnsupported = -1,
    imMemErr = -2
};

struct imStdParms {
    csSDK_int32 imInterfaceVer = 1;
    SPBasicSuite* piSuites = nullptr;
};

struct imFileInfoRec8 {
    csSDK_uint32 vidDuration = 0;
    csSDK_int32 vidScale = 0;
    csSDK_int32 vidSampleSize = 0;
    prBool hasVideo = 0;
    prBool hasAudio = 0;
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
