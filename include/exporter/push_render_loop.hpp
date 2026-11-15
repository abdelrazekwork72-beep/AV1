#pragma once
/// @file push_render_loop.hpp
/// @brief Push Model multi-pass render loop copying PPixHand inside callback.

#include "exporter/exporter_types.hpp"
#include "openmedia/video_encoder.hpp"
#include <cstdint>
#include <vector>

namespace openmedia::exporter {
/// @brief Executes 1-pass and 2-pass Push Model export loops with immediate frame copy, zero-PTS normalization, and EOF drain.
class PushRenderLoop {
public:
    /// @brief Run simulated DoMultiPassExportLoop over totalFrames for passes (1 or 2).
    static csSDK_int32 runMultiPass(uint32_t passes, uint32_t totalFrames,
                                    uint32_t width, uint32_t height,
                                    std::vector<RawVideoFrame>& outCopiedFrames);

    /// @brief Shift all frame PTS values so frames[0].pts == 0 without timestamp drift.
    static void normalizeFrameTimestamps(std::vector<RawVideoFrame>& frames) noexcept;

    /// @brief Run multi-pass loop with initialPtsOffset, normalize PTS to 0, and drain 100% of EOF tail frames.
    static csSDK_int32 runMultiPassWithDrain(uint32_t passes, uint32_t totalFrames,
                                             uint32_t width, uint32_t height,
                                             int64_t initialPtsOffset,
                                             std::vector<RawVideoFrame>& outCopiedFrames,
                                             uint32_t& outDrainedFrames,
                                             bool& outTrailerWritten);
};
} // namespace openmedia::exporter
