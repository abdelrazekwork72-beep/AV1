#pragma once
// Clean-room minimal C++17 mock header for CI/testing - zero proprietary Adobe SDK code.
#include "PrSDKTypes.h"

struct PrSDKExportProgressSuite {
    prSuiteError (*UpdateProgressPercent)(csSDK_uint32 exporterPluginID, float percentDone) = nullptr;
    prSuiteError (*SetProgressString)(csSDK_uint32 exporterPluginID, const PrSDKString* progressString) = nullptr;
};
