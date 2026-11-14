#pragma once
// Clean-room minimal C++17 mock header for CI/testing - zero proprietary Adobe SDK code.
#include "PrSDKPixelFormat.h"
#include "PrSDKTypes.h"

struct PrSDKPPixCreatorSuite {
    prSuiteError (*CreatePPix)(PPixHand* outPPix, csSDK_int32 access, PrPixelFormat format, const prRect* bounds) = nullptr;
    prSuiteError (*CreateDiskAlignedPPix)(PPixHand* outPPix, csSDK_int32 access, PrPixelFormat format, const prRect* bounds) = nullptr;
};
