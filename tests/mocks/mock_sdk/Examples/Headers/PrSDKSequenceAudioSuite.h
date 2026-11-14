#pragma once
// Clean-room minimal C++17 mock header for CI/testing - zero proprietary Adobe SDK code.
#include "PrSDKAudioSuite.h"
#include "PrSDKTypes.h"

struct PrSDKSequenceAudioSuite {
    prSuiteError (*MakeAudioRenderer)(csSDK_uint32 pluginID, PrTime startTime, PrAudioChannelType channelType, PrAudioSampleType sampleType, float sampleRate, csSDK_uint32* outAudioRendererID) = nullptr;
    prSuiteError (*ReleaseAudioRenderer)(csSDK_uint32 pluginID, csSDK_uint32 audioRendererID) = nullptr;
    prSuiteError (*GetAudio)(csSDK_uint32 audioRendererID, csSDK_uint32 numSampleFrames, float** outBuffer, prBool clip) = nullptr;
};
