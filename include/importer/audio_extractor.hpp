#pragma once
/// @file audio_extractor.hpp
/// @brief Zero-conform 32-bit float planar audio extractor for imImportAudio7.

#include "importer/importer_types.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace openmedia::importer {

/// @brief Extracts interleaved PCM16/Opus/FLAC/Vorbis frames into 32-bit planar float arrays.
class AudioExtractor {
public:
    /// @brief Maximum allowed samples per single imImportAudio7 call (TM-2.4.1 DoS cap).
    static constexpr size_t kMaxAudioBatchSamples = 65536U;

    /// @brief Handle selector imImportAudio7 by converting interleaved PCM16 to planar float.
    static csSDK_int32 importAudio7(ImImportAudioRec7Ext* rec,
                                    const std::vector<int16_t>& interleavedPcm16,
                                    uint32_t channels) noexcept;
};

} // namespace openmedia::importer
