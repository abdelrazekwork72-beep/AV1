#pragma once
// Clean-room C++17 Adobe Premiere Pro MediaCore Exporter ABI header.
#include "PrSDKPixelFormat.h"
#include "PrSDKTypes.h"

enum exSelector : csSDK_int32 {
    exSelStartup = 0,
    exSelBeginInstance = 1,
    exSelGenerateDefaultParams = 2,
    exSelPostProcessParams = 3,
    exSelGetParamSummary = 4,
    exSelQueryOutputSettings = 5,
    exSelQueryExportFileExtension = 6,
    exSelParamButton = 7,
    exSelEndInstance = 8,
    exSelExport = 9,
    exSelQueryStillSequence = 10,
    exSelShutdown = 11,
    exSelValidateParamChanged = 12,
    exSelValidateOutputSettings = 13,
    exSelQueryOutputFileList = 14
};

enum exReturnValue : csSDK_int32 {
    exportReturn_ErrNone = 0,
    exportReturn_IterateExporter = 1001,
    exportReturn_IterateExporterDone = 1002,
    exportReturn_Abort = 1003,
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
