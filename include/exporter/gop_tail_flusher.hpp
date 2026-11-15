#pragma once
/// @file gop_tail_flusher.hpp
/// @brief Drains delayed lookahead/B-frames and finalizes container trailer/Cues.
#include "exporter/exporter_types.hpp"
#include "openmedia/core_types.hpp"
#include <cstdint>
#include <vector>

namespace openmedia::exporter {
/// @brief Drains delayed encoder packets and writes final container Cues/SeekHead.
class GopTailFlusher {
public:
    /// @brief Flush pending delayed frames and finalize trailer.
    static csSDK_int32 flushAndFinalize(uint32_t delayedFrames, uint32_t& outDrained, bool& outTrailerWritten) noexcept;

    /// @brief Drain 100% of trailing frames at sequence EOF so WebM/MKV never drops the final frame.
    static csSDK_int32 drainAtEof(std::vector<openmedia::RawVideoFrame>& pendingTailFrames,
                                  std::vector<openmedia::RawVideoFrame>& outDrainedFrames,
                                  bool& outTrailerWritten);
};
} // namespace openmedia::exporter
