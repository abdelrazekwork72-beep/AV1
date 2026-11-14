#include "exporter/push_render_loop.hpp"
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
} // namespace openmedia::exporter
