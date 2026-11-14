#include "exporter/pull_sequence_renderer.hpp"
namespace openmedia::exporter {
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
} // namespace openmedia::exporter
