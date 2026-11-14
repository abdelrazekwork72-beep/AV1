#include "importer/peak_waveform_generator.hpp"

#include <algorithm>

namespace openmedia::importer {

csSDK_int32 PeakWaveformGenerator::generatePeaks(const std::vector<float>& samples,
                                                 size_t samplesPerPeak,
                                                 std::vector<float>& outMins,
                                                 std::vector<float>& outMaxs) {
    if (samplesPerPeak == 0 || samples.empty()) {
        outMins.clear();
        outMaxs.clear();
        return kImMemErr;
    }
    const size_t binCount = (samples.size() + samplesPerPeak - 1U) / samplesPerPeak;
    outMins.resize(binCount);
    outMaxs.resize(binCount);
    for (size_t b = 0; b < binCount; ++b) {
        const size_t start = b * samplesPerPeak;
        const size_t end = std::min(start + samplesPerPeak, samples.size());
        float mn = samples[start];
        float mx = samples[start];
        for (size_t i = start + 1; i < end; ++i) {
            mn = std::min(mn, samples[i]);
            mx = std::max(mx, samples[i]);
        }
        outMins[b] = mn;
        outMaxs[b] = mx;
    }
    return kImNoErr;
}

} // namespace openmedia::importer
