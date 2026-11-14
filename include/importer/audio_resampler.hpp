#pragma once
/// @file audio_resampler.hpp
/// @brief High-precision audio resampler (44.1kHz / 96kHz -> 48kHz) with zero clock skew.

#include <cstdint>
#include <vector>

namespace openmedia::importer {

/// @brief Resamples float PCM buffers between arbitrary sample rates with exact rational ratio.
class AudioResampler {
public:
    /// @brief Resample a single-channel float buffer from srcRate to dstRate.
    /// @param input Input float samples.
    /// @param srcRate Source sample rate in Hz.
    /// @param dstRate Destination sample rate in Hz.
    /// @param output Receives resampled float samples.
    /// @return true on success, false on invalid sample rates.
    static bool resample(const std::vector<float>& input,
                         uint32_t srcRate,
                         uint32_t dstRate,
                         std::vector<float>& output);
};

} // namespace openmedia::importer
