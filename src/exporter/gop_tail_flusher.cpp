#include "exporter/gop_tail_flusher.hpp"
#include <utility>

namespace openmedia::exporter {

csSDK_int32 GopTailFlusher::flushAndFinalize(uint32_t delayedFrames, uint32_t& outDrained, bool& outTrailerWritten) noexcept {
    outDrained = delayedFrames;
    outTrailerWritten = true;
    return kExNoErr;
}

csSDK_int32 GopTailFlusher::drainAtEof(std::vector<openmedia::RawVideoFrame>& pendingTailFrames,
                                       std::vector<openmedia::RawVideoFrame>& outDrainedFrames,
                                       bool& outTrailerWritten) {
    outDrainedFrames.reserve(outDrainedFrames.size() + pendingTailFrames.size());
    for (auto& frame : pendingTailFrames) {
        outDrainedFrames.push_back(std::move(frame));
    }
    pendingTailFrames.clear();
    outTrailerWritten = true;
    return kExNoErr;
}

} // namespace openmedia::exporter
