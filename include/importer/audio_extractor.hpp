#pragma once
/// @file audio_extractor.hpp
/// @brief Zero-conform 32-bit float planar audio extractor for imImportAudio7.

#include "importer/importer_types.hpp"

#include <cstdint>
#include <vector>

namespace openmedia::importer {

/// @brief Extracts interleaved PCM16/Opus/FLAC/Vorbis frames into 32-bit planar float arrays.
class AudioExtractor {
public:
    /// @brief Handle selector imImportAudio7 by converting interleaved PCM16 to planar float.
    /// @param rec Extended imImportAudioRec7 record with valid buffer pointers.
    /// @param interleavedPcm16 Source interleaved 16-bit signed PCM samples.
    /// @param channels Channel count (1..32).
    /// @return kImNoErr on success, kImMemErr on invalid pointers or channel count.
    static csSDK_int32 importAudio7(ImImportAudioRec7Ext* rec,
                                    const std::vector<int16_t>& interleavedPcm16,
                                    uint32_t channels) noexcept;
};

} // namespace openmedia::importer
