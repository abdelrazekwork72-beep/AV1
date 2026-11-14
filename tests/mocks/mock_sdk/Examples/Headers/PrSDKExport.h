#pragma once
// Clean-room minimal C++17 mock header for CI/testing - zero proprietary Adobe SDK code.
#include "PrSDKPixelFormat.h"
#include "PrSDKTypes.h"

enum exSelector : csSDK_int32 {
    exSelStartup = 0,
    exSelShutdown = 1,
    exSelBeginInstance = 2,
    exSelEndInstance = 3,
    exSelExport = 4,
    exSelQueryOutputFileList = 5,
    exSelValidateParamChanged = 6,
    exSelGenerateDefaultParams = 7
};

enum exReturnValue : csSDK_int32 {
    exportReturn_ErrNone = 0,
    exportReturn_Abort = 1,
    exportReturn_InternalError = -1
};

struct exExporterInfoRec {
    csSDK_uint32 exporterPluginID = 0;
    csSDK_uint32 fileType = 0;
    csSDK_uint32 interfaceVersion = 1;
};

struct exDoExportRec {
    csSDK_uint32 exporterPluginID = 0;
    PrTime startTime = 0;
    PrTime endTime = 0;
    prBool exportVideo = 1;
    prBool exportAudio = 1;
};

struct exportStdParms {
    csSDK_int32 exportParamVersion = 1;
    void* spBasic = nullptr;
};
