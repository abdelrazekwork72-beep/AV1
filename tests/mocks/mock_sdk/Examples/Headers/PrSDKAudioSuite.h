#pragma once
// Clean-room minimal C++17 mock header for CI/testing - zero proprietary Adobe SDK code.
#include "PrSDKTypes.h"

enum PrAudioChannelType : csSDK_int32 {
    kPrAudioChannelType_Mono = 1,
    kPrAudioChannelType_Stereo = 2,
    kPrAudioChannelType_51 = 6
};

enum PrAudioSampleType : csSDK_int32 {
    kPrAudioSampleType_32BitFloat = 0,
    kPrAudioSampleType_16BitInt = 1,
    kPrAudioSampleType_24BitInt = 2
};

struct PrSDKAudioSuite {
    prSuiteError (*ConvertAndInterleaveTo32BitFloat)(float** src, float* dst, csSDK_uint32 numChannels, csSDK_uint32 numSampleFrames) = nullptr;
    prSuiteError (*UninterleaveAndConvertFrom32BitFloat)(const float* src, float** dst, csSDK_uint32 numChannels, csSDK_uint32 numSampleFrames) = nullptr;
};
