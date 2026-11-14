#pragma once
// Clean-room minimal C++17 mock header for CI/testing - zero proprietary Adobe SDK code.
#include "PrSDKTypes.h"

#if defined(_WIN32)
#define PREMPLUGENTRY __declspec(dllexport)
#else
#define PREMPLUGENTRY __attribute__((visibility("default")))
#endif
