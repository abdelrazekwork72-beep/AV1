#pragma once
/// @file cpu_codecs.hpp
/// @brief Software CPU codec registry (libsvtav1, libaom-av1, libvpx-vp9, libopus, libflac).
#include "openmedia/core_types.hpp"
namespace openmedia::exporter {
/// @brief Resolves software encoder backend names and validates bit-depth/chroma support.
class CpuCodecBackends {
public:
    /// @brief Return canonical FFmpeg encoder name for software CodecId.
    static const char* selectEncoderName(CodecId codec, bool preferSpeed) noexcept;
    /// @brief Validate 8/10/12-bit and 4:2:0/4:2:2/4:4:4 combinations.
    static bool supportsFormat(CodecId codec, int32_t bitDepth, int32_t chroma) noexcept;
};
} // namespace openmedia::exporter
