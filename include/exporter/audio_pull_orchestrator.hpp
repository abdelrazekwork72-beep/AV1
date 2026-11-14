#pragma once
/// @file audio_pull_orchestrator.hpp
/// @brief Pulls 32-bit float planar audio and interleaves A/V packets.

#include "exporter/exporter_types.hpp"

#include <cstdint>
#include <vector>

namespace openmedia::exporter {

/// @brief Orchestrates PrSDKSequenceAudioSuite::GetAudio pulls and A/V packet interleaving.
class AudioPullOrchestrator {
public:
    /// @brief Interleave planar float audio channels into a packed float buffer.
    static csSDK_int32 interleavePlanar(const std::vector<std::vector<float>>& planar,
                                        std::vector<float>& outPacked);
};

} // namespace openmedia::exporter
