#pragma once
// Clean-room minimal C++17 mock header for CI/testing - zero proprietary Adobe SDK code.
#include "PrSDKTypes.h"

struct PrSDKErrorSuite3 {
    prSuiteError (*SetEventString)(csSDK_uint32 eventType, const char* title, const char* description) = nullptr;
};

using PrSDKErrorSuite = PrSDKErrorSuite3;
