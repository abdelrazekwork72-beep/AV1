#pragma once
// Clean-room minimal C++17 mock header for CI/testing - zero proprietary Adobe SDK code.
#include "PrSDKTypes.h"

enum PrPixelFormat : csSDK_uint32 {
    PrPixelFormat_Invalid = 0,
    PrPixelFormat_BGRA_4444_8u = 0x42475241,
    PrPixelFormat_VUYA_4444_8u = 0x56555941,
    PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u = 0x59563132,
    PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u = 0x59563133,
    PrPixelFormat_RGB_444_10u = 0x5231304B
};

constexpr PrPixelFormat PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_709 = PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u;
