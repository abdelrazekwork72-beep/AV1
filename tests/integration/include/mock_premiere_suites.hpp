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
#include "openmedia/core_types.hpp"
#include "importer/importer_types.hpp"
#include "exporter/exporter_types.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace openmedia::qa {

/// @brief Documented base contract scrub benchmark report (Module 4 README Section 3).
struct ScrubBenchmarkReport {
    double minLatencyMs{0.0};
    double p50LatencyMs{0.0};
    double p95LatencyMs{0.0};
    double p99LatencyMs{0.0};
    double ringBufferSpeedupPercent{0.0};
    uint32_t totalFramesQueried{0};
    uint32_t ppixCacheHits{0};
    uint32_t ringBufferHits{0};
};

/// @brief Documented base contract AME batch job result (Module 4 README Section 3).
struct AmeBatchJobResult {
    std::string jobName;
    std::string outputPath;
    bool presetCachePurged{false};
    bool exportSucceeded{false};
    bool containerValid{false};
    size_t memoryDeltaBytes{0};
};

/// @brief Documented abstract SweetPea host interface (Module 4 README Section 3).
class IMockSweetPeaHost {
public:
    virtual ~IMockSweetPeaHost() = default;
    virtual void* getSPBasicSuite() = 0;
    virtual bool purgeAmePresetCache(const std::string& ameAppDataDir) = 0;
};

} // namespace openmedia::qa

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
struct MockPremiereHost : public openmedia::qa::IMockSweetPeaHost {
    MockSPBasicSuite basicSuite{};
    imStdParms importerStdParms{};
    exportStdParms exporterStdParms{};
    PrSDKMemorySuite memorySuite{};
    PrSDKPPixSuite ppixSuite{};
    PrSDKPPixCreatorSuite ppixCreatorSuite{};
    PrSDKPPixCacheSuite ppixCacheSuite{};
    PrSDKAudioSuite audioSuite{};

    MockPremiereHost() noexcept {
        importerStdParms = imStdParms{};
        exporterStdParms = exportStdParms{};
        importerStdParms.piSuites = nullptr;
        exporterStdParms.spBasic = &basicSuite;
    }

    void* getSPBasicSuite() override {
        return &basicSuite;
    }

    bool purgeAmePresetCache(const std::string& ameAppDataDir) override {
        return !ameAppDataDir.empty();
    }
};

} // namespace openmedia::integration
