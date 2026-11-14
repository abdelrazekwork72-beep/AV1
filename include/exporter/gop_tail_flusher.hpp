#pragma once
/// @file gop_tail_flusher.hpp
/// @brief Drains delayed lookahead/B-frames and finalizes container trailer/Cues.
#include "exporter/exporter_types.hpp"
#include <cstdint>
namespace openmedia::exporter {
/// @brief Drains delayed encoder packets and writes final container Cues/SeekHead.
class GopTailFlusher {
public:
    /// @brief Flush pending delayed frames and finalize trailer.
    static csSDK_int32 flushAndFinalize(uint32_t delayedFrames, uint32_t& outDrained, bool& outTrailerWritten) noexcept;
};
} // namespace openmedia::exporter
