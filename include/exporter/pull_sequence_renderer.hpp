#pragma once
/// @file pull_sequence_renderer.hpp
/// @brief Pull Model sequence renderer via RenderVideoFrameAndConformToPixelFormat.
#include "exporter/exporter_types.hpp"
#include "openmedia/core_types.hpp"
namespace openmedia::exporter {
/// @brief Pulls and conforms individual sequence frames on demand.
class PullSequenceRenderer {
public:
    /// @brief Render and conform a single frame at frameIndex.
    static csSDK_int32 renderFrame(int64_t frameIndex, int32_t width, int32_t height,
                                   PixelFormat targetFmt, RawVideoFrame& outFrame);
};
} // namespace openmedia::exporter
