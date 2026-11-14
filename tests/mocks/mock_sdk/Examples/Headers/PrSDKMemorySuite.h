#pragma once
// Clean-room minimal C++17 mock header for CI/testing - zero proprietary Adobe SDK code.
#include "PrSDKTypes.h"

struct PrSDKMemoryManagerSuite {
    void* (*NewPtr)(csSDK_uint32 byteCount) = nullptr;
    void (*PrDisposePtr)(void* ptr) = nullptr;
    csSDK_uint32 (*GetPtrSize)(void* ptr) = nullptr;
};

using PrSDKMemorySuite = PrSDKMemoryManagerSuite;
