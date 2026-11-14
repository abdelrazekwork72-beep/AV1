#pragma once
// Clean-room minimal C++17 mock header for CI/testing - zero proprietary Adobe SDK code.
#include "PrSDKTypes.h"

struct PrSDKCaptioningSuite {
    prSuiteError (*HasCaptions)(csSDK_uint32 exporterPluginID, prBool* outHasCaptions) = nullptr;
    prSuiteError (*GetCaptionStreamCount)(csSDK_uint32 exporterPluginID, csSDK_int32* outCount) = nullptr;
};
