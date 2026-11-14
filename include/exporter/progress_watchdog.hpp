#pragma once
/// @file progress_watchdog.hpp
/// @brief Export progress reporter handling suiteError_ExporterSuspended and GOP-closing abort.
#include "exporter/exporter_types.hpp"
#include <cstdint>
namespace openmedia::exporter {
class ProgressWatchdog {
public:
    static constexpr uint32_t kMaxGopClosingFrames = 4U;
    csSDK_int32 update(double percent, bool suspended, bool userAbort, bool isGopBoundary) noexcept;
    uint32_t resumeCount() const noexcept { return resumeCount_; }
    uint32_t closingFramesUsed() const noexcept { return gopGraceFrames_; }
    bool isAborted() const noexcept { return aborted_; }
private:
    uint32_t resumeCount_ = 0;
    uint32_t gopGraceFrames_ = 0;
    bool aborted_ = false;
};
} // namespace openmedia::exporter
