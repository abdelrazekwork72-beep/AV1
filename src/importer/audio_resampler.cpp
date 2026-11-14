#include "importer/audio_resampler.hpp"

#include <algorithm>
#include <cmath>

namespace openmedia::importer {

bool AudioResampler::resample(const std::vector<float>& input,
                              uint32_t srcRate,
                              uint32_t dstRate,
                              std::vector<float>& output) {
    if (srcRate == 0 || dstRate == 0) {
        return false;
    }
    if (input.empty()) {
        output.clear();
        return true;
    }
    if (srcRate == dstRate) {
        output = input;
        return true;
    }
    const size_t outCount = static_cast<size_t>(
        (static_cast<uint64_t>(input.size()) * dstRate + (srcRate / 2U)) / srcRate);
    output.resize(outCount);
    const double ratio = static_cast<double>(srcRate) / static_cast<double>(dstRate);
    for (size_t i = 0; i < outCount; ++i) {
        const double srcPos = static_cast<double>(i) * ratio;
        const size_t idx0 = std::min(static_cast<size_t>(srcPos), input.size() - 1U);
        const size_t idx1 = std::min(idx0 + 1U, input.size() - 1U);
        const double frac = srcPos - static_cast<double>(idx0);
        output[i] = static_cast<float>(input[idx0] * (1.0 - frac) + input[idx1] * frac);
    }
    return true;
}

} // namespace openmedia::importer
