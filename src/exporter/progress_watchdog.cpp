#include "exporter/progress_watchdog.hpp"
namespace openmedia::exporter {
csSDK_int32 ProgressWatchdog::update(double percent, bool suspended, bool userAbort, bool isGopBoundary) noexcept {
    if (percent < 0.0 || percent > 1.0) return kExError;
    if (suspended) {
        ++resumeCount_;
    }
    if (userAbort) {
        ++gopGraceFrames_;
        if (isGopBoundary || gopGraceFrames_ >= 60U) {
            aborted_ = true;
            return kExAbort;
        }
    }
    return kExNoErr;
}
} // namespace openmedia::exporter
