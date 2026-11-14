#include "importer/audio_resampler.hpp"

#include <algorithm>
#include <cmath>

namespace openmedia::importer {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kHalfTaps = 24;

double blackmanHarris(double n, double totalN) noexcept {
    constexpr double a0 = 0.35875;
    constexpr double a1 = 0.48829;
    constexpr double a2 = 0.14128;
    constexpr double a3 = 0.01168;
    const double phase = (2.0 * kPi * n) / totalN;
    return a0 - a1 * std::cos(phase) + a2 * std::cos(2.0 * phase) - a3 * std::cos(3.0 * phase);
}

double sinc(double x) noexcept {
    if (std::fabs(x) < 1e-12) {
        return 1.0;
    }
    const double pix = kPi * x;
    return std::sin(pix) / pix;
}

float evalWindowedSinc(const std::vector<float>& input, double srcPos, double cutoff) noexcept {
    const int64_t center = static_cast<int64_t>(std::floor(srcPos));
    const int64_t nSamples = static_cast<int64_t>(input.size());
    double sum = 0.0;
    double weightSum = 0.0;
    constexpr double kTotalWidth = static_cast<double>(2 * kHalfTaps);

    for (int k = -kHalfTaps; k <= kHalfTaps; ++k) {
        const int64_t idx = std::clamp<int64_t>(center + k, 0, nSamples - 1);
        const double delta = srcPos - static_cast<double>(center + k);
        const double win = blackmanHarris(delta + kHalfTaps, kTotalWidth);
        const double coeff = sinc(delta * cutoff) * win * cutoff;
        sum += static_cast<double>(input[static_cast<size_t>(idx)]) * coeff;
        weightSum += coeff;
    }
    return static_cast<float>((std::fabs(weightSum) > 1e-12) ? (sum / weightSum) : sum);
}

} // namespace

int64_t AudioResampler::computeExactSampleCount(int64_t inputSamples,
                                                uint32_t srcRate,
                                                uint32_t dstRate) noexcept {
    if (inputSamples <= 0 || srcRate == 0 || dstRate == 0) {
        return 0;
    }
    const uint64_t uIn = static_cast<uint64_t>(inputSamples);
    const uint64_t q = uIn / srcRate;
    const uint64_t r = uIn % srcRate;
    return static_cast<int64_t>(q * dstRate + (r * dstRate + (srcRate / 2U)) / srcRate);
}

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
        computeExactSampleCount(static_cast<int64_t>(input.size()), srcRate, dstRate));
    output.resize(outCount);
    const double ratio = static_cast<double>(srcRate) / static_cast<double>(dstRate);
    const double cutoff = (dstRate < srcRate) ? (static_cast<double>(dstRate) / static_cast<double>(srcRate)) : 1.0;

    for (size_t i = 0; i < outCount; ++i) {
        output[i] = evalWindowedSinc(input, static_cast<double>(i) * ratio, cutoff);
    }
    return true;
}

} // namespace openmedia::importer
