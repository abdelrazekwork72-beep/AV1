#include "exporter/gop_tail_flusher.hpp"
namespace openmedia::exporter {
csSDK_int32 GopTailFlusher::flushAndFinalize(uint32_t delayedFrames, uint32_t& outDrained, bool& outTrailerWritten) noexcept {
    outDrained = delayedFrames;
    outTrailerWritten = true;
    return kExNoErr;
}
} // namespace openmedia::exporter
