#include "exporter/pull_sequence_renderer.hpp"
namespace openmedia::exporter {

int64_t PullSequenceRenderer::normalizePts(int64_t rawPts, int64_t firstFramePts) noexcept {
    return (rawPts > firstFramePts) ? (rawPts - firstFramePts) : 0;
}

csSDK_int32 PullSequenceRenderer::renderFrame(int64_t frameIndex, int32_t width, int32_t height,
                                              PixelFormat targetFmt, RawVideoFrame& outFrame) {
    if (frameIndex < 0 || width <= 0 || height <= 0) return kExError;
    outFrame.width = width;
    outFrame.height = height;
    outFrame.format = targetFmt;
    outFrame.pts = frameIndex;
    outFrame.rowBytes[0] = RawVideoFrame::alignRowStride16(static_cast<uint32_t>(width) * 4U);
    outFrame.planes[0].assign(static_cast<size_t>(outFrame.rowBytes[0]) * static_cast<size_t>(height), 0U);
    return kExNoErr;
}

csSDK_int32 PullSequenceRenderer::renderSequenceFrame(int64_t frameIndex, int64_t sequenceStartFrame,
                                                      int32_t width, int32_t height,
                                                      PixelFormat targetFmt, RawVideoFrame& outFrame) {
    const csSDK_int32 status = renderFrame(frameIndex, width, height, targetFmt, outFrame);
    if (status == kExNoErr) {
        outFrame.pts = normalizePts(frameIndex, sequenceStartFrame);
    }
    return status;
}

} // namespace openmedia::exporter
