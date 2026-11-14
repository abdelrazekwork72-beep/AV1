#pragma once
// Clean-room minimal C++17 mock header for CI/testing - zero proprietary Adobe SDK code.
#include <cstddef>
#include <cstdint>

using csSDK_int32 = std::int32_t;
using csSDK_uint32 = std::uint32_t;
using csSDK_int64 = std::int64_t;
using csSDK_uint64 = std::uint64_t;
using csSDK_size_t = std::size_t;
using prBool = std::uint8_t;
using prSuiteError = std::int32_t;
using PrTime = std::int64_t;
using PPixHand = void*;

constexpr prSuiteError suiteError_NoError = 0;
constexpr prSuiteError suiteError_Fail = -1;
constexpr prSuiteError suiteError_InvalidParms = -2;
constexpr prSuiteError suiteError_OutOfMemory = -3;

struct prRect {
    csSDK_int32 left = 0;
    csSDK_int32 top = 0;
    csSDK_int32 right = 0;
    csSDK_int32 bottom = 0;
};

struct prPoint {
    csSDK_int32 x = 0;
    csSDK_int32 y = 0;
};

struct PrSDKString {
    std::uint8_t opaque[64]{};
};

struct SPBasicSuite {
    prSuiteError (*AcquireSuite)(const char* name, csSDK_int32 version, const void** suite) = nullptr;
    prSuiteError (*ReleaseSuite)(const char* name, csSDK_int32 version) = nullptr;
};
