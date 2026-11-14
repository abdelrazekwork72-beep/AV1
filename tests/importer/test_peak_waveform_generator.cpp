#include "importer/peak_waveform_generator.hpp"

#include <cassert>
#include <chrono>
#include <iostream>
#include <vector>

using namespace openmedia::importer;

int main() {
    std::vector<float> samples(480000, 0.25f);
    samples[10] = -0.9f;
    samples[20] = 0.95f;
    std::vector<float> mins, maxs;

    const auto t0 = std::chrono::steady_clock::now();
    assert(PeakWaveformGenerator::generatePeaks(samples, 480, mins, maxs) == kImNoErr);
    const auto t1 = std::chrono::steady_clock::now();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    assert(mins.size() == 1000);
    assert(mins[0] == -0.9f);
    assert(maxs[0] == 0.95f);
    assert(ms < 50);

    std::cout << "[PASS] TC-2.4.3 PeakWaveformGenerator in-memory peaks in " << ms << " ms.\n";
    return 0;
}
