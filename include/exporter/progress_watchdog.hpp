#pragma once
/// @file progress_watchdog.hpp
/// @brief Export progress reporter handling suiteError_ExporterSuspended and GOP-closing abort.
#include "exporter/exporter_types.hpp"
#include <cstdint>
namespace openmedia::exporter {
/// @brief Manages progress updates, suspend/resume, and bounded GOP-closing abort.
class ProgressWatchdog {
public:
    /// @brief Update progress and handle suspend/abort states.
    csSDK_int32 update(double percent, bool suspended, bool userAbort, bool isGopBoundary) noexcept;
    /// @brief Return number of WaitForResume invocations.
    uint32_t resumeCount() const noexcept { return resumeCount_; }
    /// @brief Query whether abort has finalized at a GOP boundary.
    bool isAborted() const noexcept { return aborted_; }
private:
    uint32_t resumeCount_ = 0;
    uint32_t gopGraceFrames_ = 0;
    bool aborted_ = false;
};
} // namespace openmedia::exporter
