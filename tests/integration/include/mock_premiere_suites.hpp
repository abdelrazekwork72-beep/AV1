#pragma once
/// @file mock_premiere_suites.hpp
/// @brief Base contract providing clean-room SweetPea / MediaCore host mock suites
///        for Module 4 headless integration, scrub benchmarking, and AME batch tests.

#include "PrSDKAudioSuite.h"
#include "PrSDKExport.h"
#include "PrSDKImport.h"
#include "PrSDKMemorySuite.h"
#include "PrSDKPPixCacheSuite.h"
#include "PrSDKPPixCreatorSuite.h"
#include "PrSDKPPixSuite.h"
#include "PrSDKPixelFormat.h"
#include "PrSDKTypes.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

/// @brief Minimal clean-room exportStdParms definition matching Adobe MediaCore Exporter ABI.
struct exportStdParms {
    csSDK_int32 exInterfaceVer = 1;
    SPBasicSuite* spBasic = nullptr;
};

namespace openmedia::integration {

/// @brief Cross-platform dynamic plugin entry point function signatures.
using ImportEntryFn = csSDK_int32 (*)(csSDK_int32 selector,
                                      imStdParms* stdParms,
                                      void* param1,
                                      void* param2);

using ExportEntryFn = csSDK_int32 (*)(csSDK_int32 selector,
                                      exportStdParms* stdParms,
                                      void* param1,
                                      void* param2);

/// @brief Minimal SweetPea SPBasicSuite mock for headless host suite acquisition.
struct MockSPBasicSuite {
    SPBasicSuite base{};

    static prSuiteError acquireSuite(const char* name,
                                     csSDK_int32 /*version*/,
                                     const void** suiteOut) noexcept {
        if (name == nullptr || suiteOut == nullptr) {
            return suiteError_InvalidParms;
        }
        *suiteOut = nullptr;
        return suiteError_NoError;
    }

    static prSuiteError releaseSuite(const char* name,
                                     csSDK_int32 /*version*/) noexcept {
        return (name != nullptr) ? suiteError_NoError : suiteError_InvalidParms;
    }

    MockSPBasicSuite() noexcept {
        base.AcquireSuite = &acquireSuite;
        base.ReleaseSuite = &releaseSuite;
    }
};

/// @brief Headless mock host environment bundling SweetPea suites and stdParms.
struct MockPremiereHost {
    MockSPBasicSuite basicSuite{};
    imStdParms importerStdParms{};
    exportStdParms exporterStdParms{};
    PrSDKMemorySuite memorySuite{};
    PrSDKPPixSuite ppixSuite{};
    PrSDKPPixCreatorSuite ppixCreatorSuite{};
    PrSDKPPixCacheSuite ppixCacheSuite{};
    PrSDKAudioSuite audioSuite{};

    static void* mockNewPtr(csSDK_uint32 byteCount) noexcept {
        return std::calloc(1U, byteCount == 0U ? 1U : byteCount);
    }

    static void mockDisposePtr(void* ptr) noexcept {
        std::free(ptr);
    }

    MockPremiereHost() noexcept {
        importerStdParms.imInterfaceVer = 1;
        importerStdParms.piSuites = &basicSuite.base;
        exporterStdParms.exInterfaceVer = 1;
        exporterStdParms.spBasic = &basicSuite.base;
        memorySuite.NewPtr = &mockNewPtr;
        memorySuite.PrDisposePtr = &mockDisposePtr;
    }
};

} // namespace openmedia::integration
