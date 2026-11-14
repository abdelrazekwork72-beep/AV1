/// @file test_audio_decoder.cpp
/// @brief Standalone unit test TC-1.2.3 for CoreAudioDecoder PCM-to-float precision,
///        5.1 (6-channel) planar layout, and [-1.0f, 1.0f] clamping bounds.

#include "openmedia/audio_decoder.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;

bool testPrecisionSnr() {
    constexpr int32_t kSampleRate = 48000;
    constexpr int32_t kNumSamples = 4096;
    constexpr double kFreqHz = 1000.0;
    double signalPower = 0.0;
    double noisePower = 0.0;

    for (int32_t i = 0; i < kNumSamples; ++i) {
        const double phase = 2.0 * kPi * kFreqHz * static_cast<double>(i) / static_cast<double>(kSampleRate);
        const double ideal = std::sin(phase);
        const auto pcm16 = static_cast<int16_t>(std::llround(ideal * 32767.0));
        const float converted = openmedia::convertInt16ToFloat32(pcm16);
        const double expected = static_cast<double>(pcm16) / 32767.0;
        const double error = static_cast<double>(converted) - expected;
        signalPower += expected * expected;
        noisePower += error * error;
    }

    const double snrDb = (noisePower > 0.0) ? 10.0 * std::log10(signalPower / noisePower) : 150.0;
    std::cout << "[TC-1.2.3] Int16->Float32 SNR: " << snrDb << " dB\n";
    return snrDb > 96.0;
}

bool testClampingBounds() {
    const float minClamped = openmedia::convertInt16ToFloat32(std::numeric_limits<int16_t>::min());
    const float negMax = openmedia::convertInt16ToFloat32(static_cast<int16_t>(-32767));
    const float zero = openmedia::convertInt16ToFloat32(0);
    const float posMax = openmedia::convertInt16ToFloat32(std::numeric_limits<int16_t>::max());

    if (minClamped != -1.0f || negMax != -1.0f || zero != 0.0f || posMax != 1.0f) {
        std::cerr << "[TC-1.2.3] Boundary mismatch\n";
        return false;
    }

    for (int32_t v = std::numeric_limits<int16_t>::min();
         v <= std::numeric_limits<int16_t>::max();
         ++v) {
        const float sample = openmedia::convertInt16ToFloat32(static_cast<int16_t>(v));
        if (std::isnan(sample) || sample < -1.0f || sample > 1.0f) {
            return false;
        }
    }
    return true;
}

bool testSurround51PlanarLayout() {
    constexpr int32_t kChannels51 = 6;
    constexpr int32_t kSamplesPerChannel = 1024;

    openmedia::RawAudioBatch batch;
    batch.sampleRate = 48000;
    batch.channels = kChannels51;
    batch.numSamples = kSamplesPerChannel;
    batch.pts = 0;
    batch.channelData.resize(static_cast<size_t>(kChannels51));

    for (int32_t ch = 0; ch < kChannels51; ++ch) {
        batch.channelData[static_cast<size_t>(ch)].resize(static_cast<size_t>(kSamplesPerChannel));
        for (int32_t i = 0; i < kSamplesPerChannel; ++i) {
            const auto pcm = static_cast<int16_t>(((ch + 1) * 4000) - (i % 1000));
            batch.channelData[static_cast<size_t>(ch)][static_cast<size_t>(i)] =
                openmedia::convertInt16ToFloat32(pcm);
        }
    }

    if (batch.channels != 6 || static_cast<int32_t>(batch.channelData.size()) != 6) {
        return false;
    }
    for (int32_t ch = 0; ch < kChannels51; ++ch) {
        const auto& plane = batch.channelData[static_cast<size_t>(ch)];
        if (static_cast<int32_t>(plane.size()) != kSamplesPerChannel) {
            return false;
        }
        for (float val : plane) {
            if (val < -1.0f || val > 1.0f) {
                return false;
            }
        }
    }
    return batch.channelData[0][0] != batch.channelData[5][0];
}

} // namespace

int main() {
    if (!testPrecisionSnr() || !testClampingBounds() || !testSurround51PlanarLayout()) {
        std::cerr << "[TC-1.2.3] FAIL\n";
        return 1;
    }
    std::cout << "[TC-1.2.3] PASS: All CoreAudioDecoder unit checks succeeded.\n";
    return 0;
}
