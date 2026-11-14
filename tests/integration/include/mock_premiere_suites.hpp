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
#include <cstring>
#include <map>
#include <string>
#include <vector>

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
    std::map<std::string, const void*> suites;

    static csSDK_int32 acquireSuite(const char* name,
                                    csSDK_int32 /*version*/,
                                    const void** suiteOut) noexcept {
        if (!name || !suiteOut) {
            return -1;
        }
        *suiteOut = nullptr;
        return 0;
    }

    static csSDK_int32 releaseSuite(const char* /*name*/,
                                    csSDK_int32 /*version*/) noexcept {
        return 0;
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

    MockPremiereHost() noexcept {
        std::memset(&importerStdParms, 0, sizeof(importerStdParms));
        std::memset(&exporterStdParms, 0, sizeof(exporterStdParms));
        importerStdParms.piSuites = nullptr;
        exporterStdParms.spBasic = &basicSuite;
    }
};

} // namespace openmedia::integration
