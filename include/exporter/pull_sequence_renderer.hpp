#pragma once
/// @file pull_sequence_renderer.hpp
/// @brief Pull Model sequence renderer via RenderVideoFrameAndConformToPixelFormat.
#include "exporter/exporter_types.hpp"
#include "openmedia/core_types.hpp"
namespace openmedia::exporter {
/// @brief Pulls and conforms individual sequence frames on demand with zero-PTS normalization.
class PullSequenceRenderer {
public:
    /// @brief Normalize rawPts relative to firstFramePts so first-frame PTS starts cleanly at 0.
    static int64_t normalizePts(int64_t rawPts, int64_t firstFramePts) noexcept;

    /// @brief Render and conform a single frame at frameIndex.
    static csSDK_int32 renderFrame(int64_t frameIndex, int32_t width, int32_t height,
                                   PixelFormat targetFmt, RawVideoFrame& outFrame);

    /// @brief Render and conform a sequence frame with zero-normalized PTS relative to sequenceStartFrame.
    static csSDK_int32 renderSequenceFrame(int64_t frameIndex, int64_t sequenceStartFrame,
                                           int32_t width, int32_t height,
                                           PixelFormat targetFmt, RawVideoFrame& outFrame);
};
} // namespace openmedia::exporter
