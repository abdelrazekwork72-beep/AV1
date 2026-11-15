#pragma once
/// @file audio_extractor.hpp
/// @brief Zero-conform 32-bit float planar audio extractor with fault-tolerant silence fallback.

#include "importer/importer_types.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace openmedia::importer {

/// @brief Extracts interleaved PCM16/Opus/FLAC/Vorbis frames into 32-bit planar float arrays.
class AudioExtractor {
public:
    static constexpr size_t kMaxAudioBatchSamples = 65536U;

    /// @brief Handle selector imImportAudio7 by converting interleaved PCM16 to planar float.
    static csSDK_int32 importAudio7(ImImportAudioRec7Ext* rec,
                                    const std::vector<int16_t>& interleavedPcm16,
                                    uint32_t channels) noexcept;

    /// @brief Fault-tolerant audio import: outputs zero-filled silence on corrupted packets without aborting.
    static csSDK_int32 importAudio7FaultTolerant(ImImportAudioRec7Ext* rec,
                                                 const std::vector<int16_t>& interleavedPcm16,
                                                 uint32_t channels,
                                                 bool packetCorrupted) noexcept;

    /// @brief Initializes audio stream and immediately closes demuxer handles if initialization fails.
    static csSDK_int32 initializeAudioWithCleanup(ICoreDemuxer* demuxer,
                                                  uint32_t channels,
                                                  bool simulateInitFailure) noexcept;
};

} // namespace openmedia::importer
