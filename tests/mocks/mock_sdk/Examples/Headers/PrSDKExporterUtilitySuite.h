#pragma once
// Clean-room minimal C++17 mock header for CI/testing - zero proprietary Adobe SDK code.
#include "PrSDKTypes.h"

struct PrSDKExporterUtilitySuite {
    prSuiteError (*ReportIntermediateProgressForRepeatedVideoFrame)(csSDK_uint32 exporterPluginID, csSDK_int32 repeatCount) = nullptr;
    prSuiteError (*DoMultiPassExportLoop)(csSDK_uint32 exporterPluginID, PrTime startTime, PrTime endTime) = nullptr;
    prSuiteError (*GetExportSourceInfo)(csSDK_uint32 exporterPluginID, csSDK_int32 selector, void* outInfo) = nullptr;
};
