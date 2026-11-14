#pragma once
// Clean-room minimal C++17 mock header for CI/testing - zero proprietary Adobe SDK code.
#include "PrSDKPixelFormat.h"
#include "PrSDKTypes.h"

struct PrSDKPPixSuite {
    prSuiteError (*Dispose)(PPixHand ppix) = nullptr;
    prSuiteError (*GetPixels)(PPixHand ppix, csSDK_int32 requestedAccess, char** outPixels) = nullptr;
    prSuiteError (*GetBounds)(PPixHand ppix, prRect* outBounds) = nullptr;
    prSuiteError (*GetRowBytes)(PPixHand ppix, csSDK_int32* outRowBytes) = nullptr;
    prSuiteError (*GetPixelFormat)(PPixHand ppix, PrPixelFormat* outPixelFormat) = nullptr;
    prSuiteError (*GetPixelAspectRatio)(PPixHand ppix, csSDK_uint32* outNum, csSDK_uint32* outDen) = nullptr;
};
