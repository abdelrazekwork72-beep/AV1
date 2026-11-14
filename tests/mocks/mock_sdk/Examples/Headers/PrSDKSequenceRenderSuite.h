#pragma once
// Clean-room minimal C++17 mock header for CI/testing - zero proprietary Adobe SDK code.
#include "PrSDKPixelFormat.h"
#include "PrSDKTypes.h"

struct SequenceRender_ParamsRec {
    PrPixelFormat inRequestedPixelFormatArray[4]{};
    csSDK_int32 inRequestedPixelFormatArrayCount = 0;
    csSDK_int32 inWidth = 0;
    csSDK_int32 inHeight = 0;
};

struct PrSDKSequenceRenderSuite {
    prSuiteError (*MakeVideoRenderer)(csSDK_uint32 pluginID, csSDK_uint32* outVideoRendererID, PrTime frameRate) = nullptr;
    prSuiteError (*ReleaseVideoRenderer)(csSDK_uint32 pluginID, csSDK_uint32 videoRendererID) = nullptr;
    prSuiteError (*RenderVideoFrame)(csSDK_uint32 videoRendererID, PrTime time, const SequenceRender_ParamsRec* params, PPixHand* outFrame) = nullptr;
};
