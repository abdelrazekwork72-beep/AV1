#pragma once
/// @file stream_tracker.hpp
/// @brief Multi-track stream iteration and metadata population for MediaCore imGetInfo8.

#include "importer/importer_types.hpp"

#include <cstdint>
#include <vector>

namespace openmedia::importer {

/// @brief Maximum number of streams supported during multi-track iteration (DoS cap).
inline constexpr int32_t kMaxSupportedStreams = 64;

/// @brief Manages multi-track video and audio stream enumeration across repeated imGetInfo8 calls.
class StreamTracker {
public:
    /// @brief Advances stream iteration for imGetInfo8 and populates stream metadata in outInfo.
    /// @param streamIdx Zero-based stream index requested by the host.
    /// @param streams Discovered container streams from the demuxer.
    /// @param outInfo Extended imFileInfoRec8 structure populated for the active stream.
    /// @return kImIterateStreams while additional streams remain; kImBadStreamIndex when exhausted, negative, or at/beyond kMaxSupportedStreams.
    static csSDK_int32 advanceStreamIteration(int32_t streamIdx,
                                              const std::vector<StreamInfo>& streams,
                                              ImFileInfoRec8Ext& outInfo);
};

} // namespace openmedia::importer
