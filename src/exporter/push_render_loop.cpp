#include "exporter/push_render_loop.hpp"
#include "exporter/gop_tail_flusher.hpp"

namespace openmedia::exporter {

csSDK_int32 PushRenderLoop::runMultiPass(uint32_t passes, uint32_t totalFrames,
                                         uint32_t width, uint32_t height,
                                         std::vector<RawVideoFrame>& outCopiedFrames) {
    if (passes == 0 || passes > 2 || width == 0 || height == 0) return kExError;
    outCopiedFrames.clear();
    outCopiedFrames.reserve(totalFrames);
    const uint32_t alignedStride = RawVideoFrame::alignRowStride16(width * 4U);
    for (uint32_t p = 0; p < passes; ++p) {
        for (uint32_t f = 0; f < totalFrames; ++f) {
            if (p + 1 == passes) {
                RawVideoFrame copied;
                copied.width = static_cast<int32_t>(width);
                copied.height = static_cast<int32_t>(height);
                copied.pts = static_cast<int64_t>(f);
                copied.rowBytes[0] = alignedStride;
                copied.planes[0].assign(static_cast<size_t>(alignedStride) * height, 0x7FU);
                outCopiedFrames.push_back(std::move(copied));
            }
        }
    }
    return kExNoErr;
}

void PushRenderLoop::normalizeFrameTimestamps(std::vector<RawVideoFrame>& frames) noexcept {
    if (frames.empty()) {
        return;
    }
    const int64_t basePts = frames.front().pts;
    for (auto& frame : frames) {
        frame.pts = (frame.pts > basePts) ? (frame.pts - basePts) : 0;
    }
}

csSDK_int32 PushRenderLoop::runMultiPassWithDrain(uint32_t passes, uint32_t totalFrames,
                                                  uint32_t width, uint32_t height,
                                                  int64_t initialPtsOffset,
                                                  std::vector<RawVideoFrame>& outCopiedFrames,
                                                  uint32_t& outDrainedFrames,
                                                  bool& outTrailerWritten) {
    const csSDK_int32 status = runMultiPass(passes, totalFrames, width, height, outCopiedFrames);
    if (status != kExNoErr) {
        return status;
    }
    for (auto& frame : outCopiedFrames) {
        frame.pts += initialPtsOffset;
    }
    normalizeFrameTimestamps(outCopiedFrames);
    std::vector<RawVideoFrame> tailBuffer;
    if (!outCopiedFrames.empty()) {
        tailBuffer.push_back(std::move(outCopiedFrames.back()));
        outCopiedFrames.pop_back();
    }
    outDrainedFrames = static_cast<uint32_t>(tailBuffer.size());
    return GopTailFlusher::drainAtEof(tailBuffer, outCopiedFrames, outTrailerWritten);
}

} // namespace openmedia::exporter
