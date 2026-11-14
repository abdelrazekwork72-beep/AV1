#pragma once
// Clean-room minimal C++17 mock header for CI/testing - zero proprietary Adobe SDK code.
#include "PrSDKTypes.h"

struct PrSDKExportStdParamSuite {
    prSuiteError (*SetParamsVersion)(csSDK_uint32 exporterPluginID, csSDK_int32 version) = nullptr;
    prSuiteError (*AddStandardParams)(csSDK_uint32 exporterPluginID) = nullptr;
};
