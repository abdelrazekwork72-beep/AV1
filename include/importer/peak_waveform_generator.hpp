#pragma once
/// @file peak_waveform_generator.hpp
/// @brief Direct in-memory min/max waveform peak generator for imGetPeakAudio.

#include "importer/importer_types.hpp"

#include <cstdint>
#include <vector>

namespace openmedia::importer {

/// @brief Computes min/max waveform peaks directly in memory for Premiere timelines.
class PeakWaveformGenerator {
public:
    /// @brief Generate min/max peaks for a single-channel float buffer.
    /// @param samples Input normalized float samples.
    /// @param samplesPerPeak Number of samples aggregated per peak bin.
    /// @param outMins Output minimum sample values per bin.
    /// @param outMaxs Output maximum sample values per bin.
    /// @return kImNoErr on success, kImMemErr on invalid arguments.
    static csSDK_int32 generatePeaks(const std::vector<float>& samples,
                                     size_t samplesPerPeak,
                                     std::vector<float>& outMins,
                                     std::vector<float>& outMaxs);
};

} // namespace openmedia::importer
