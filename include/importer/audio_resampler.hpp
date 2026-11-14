#pragma once
/// @file audio_resampler.hpp
/// @brief High-precision windowed sinc audio resampler (44.1kHz / 96kHz -> 48kHz) with SNR > 96 dB.

#include <cstdint>
#include <vector>

namespace openmedia::importer {

/// @brief Resamples float PCM buffers between arbitrary sample rates with >96 dB SNR and zero drift.
class AudioResampler {
public:
    /// @brief Compute exact target sample count over long durations without cumulative drift.
    static int64_t computeExactSampleCount(int64_t inputSamples, uint32_t srcRate, uint32_t dstRate) noexcept;

    /// @brief Resample a single-channel float buffer from srcRate to dstRate using windowed sinc filter.
    static bool resample(const std::vector<float>& input,
                         uint32_t srcRate,
                         uint32_t dstRate,
                         std::vector<float>& output);
};

} // namespace openmedia::importer
