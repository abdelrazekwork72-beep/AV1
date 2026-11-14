#include "importer/audio_resampler.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

using namespace openmedia::importer;

int main() {
    constexpr uint32_t kSrcRate = 44100;
    constexpr uint32_t kDstRate = 48000;
    constexpr double kPi = 3.14159265358979323846;
    constexpr double kFreq = 440.0;

    std::vector<float> in441(kSrcRate / 10, 0.0f);
    for (size_t i = 0; i < in441.size(); ++i) {
        in441[i] = static_cast<float>(std::sin(2.0 * kPi * kFreq * static_cast<double>(i) / kSrcRate));
    }
    std::vector<float> out48;
    assert(AudioResampler::resample(in441, kSrcRate, kDstRate, out48));
    assert(out48.size() == kDstRate / 10);

    double sigPower = 0.0;
    double errPower = 0.0;
    for (size_t i = 128; i + 128 < out48.size(); ++i) {
        const double ref = std::sin(2.0 * kPi * kFreq * static_cast<double>(i) / kDstRate);
        const double err = static_cast<double>(out48[i]) - ref;
        sigPower += ref * ref;
        errPower += err * err;
    }
    const double snrDb = 10.0 * std::log10(sigPower / std::max(errPower, 1e-20));
    assert(snrDb > 96.0);

    constexpr int64_t kFourHours441 = 14400LL * 44100LL;
    constexpr int64_t kFourHours480 = 14400LL * 48000LL;
    assert(AudioResampler::computeExactSampleCount(kFourHours441, kSrcRate, kDstRate) == kFourHours480);

    std::cout << "[PASS] TC-2.4.5 AudioResampler SNR=" << snrDb << " dB (>96 dB) & 4h zero-drift passed.\n";
    return 0;
}
