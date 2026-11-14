#pragma once
// Clean-room minimal C++17 mock header for CI/testing - zero proprietary Adobe SDK code.
#include "PrSDKTypes.h"

struct PrSDKPPixCacheSuite {
    prSuiteError (*AddFrameToCache)(csSDK_uint32 importerID, csSDK_int32 streamIdx, PPixHand frame, PrTime time) = nullptr;
    prSuiteError (*GetFrameFromCache)(csSDK_uint32 importerID, csSDK_int32 streamIdx, PrTime time, PPixHand* outFrame) = nullptr;
    prSuiteError (*RegisterCache)(csSDK_uint32 importerID) = nullptr;
};
