/// @file test_audio_decoder.cpp
/// @brief Standalone unit test TC-1.2.3 for CoreAudioDecoder PCM-to-float precision,
///        5.1 (6-channel) planar layout, [-1.0f, 1.0f] clamping bounds, and decoder lifecycle.

#include "openmedia/audio_decoder.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <utility>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;

bool isSampleInUnitRange(float sample) noexcept {
    if (std::isnan(sample)) {
        return false;
    }
    return sample >= -1.0f && sample <= 1.0f;
}

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

bool testExactBoundaryValues() {
    const float minVal = openmedia::convertInt16ToFloat32(std::numeric_limits<int16_t>::min());
    const float negMax = openmedia::convertInt16ToFloat32(static_cast<int16_t>(-32767));
    const float zero = openmedia::convertInt16ToFloat32(0);
    const float posMax = openmedia::convertInt16ToFloat32(std::numeric_limits<int16_t>::max());
    if (minVal != -1.0f || negMax != -1.0f) {
        return false;
    }
    return zero == 0.0f && posMax == 1.0f;
}

bool testAllInt16ValuesInBounds() {
    for (int32_t v = std::numeric_limits<int16_t>::min();
         v <= std::numeric_limits<int16_t>::max();
         ++v) {
        const float sample = openmedia::convertInt16ToFloat32(static_cast<int16_t>(v));
        if (!isSampleInUnitRange(sample)) {
            return false;
        }
    }
    return true;
}

bool testClampingBounds() {
    if (!testExactBoundaryValues()) {
        std::cerr << "[TC-1.2.3] Boundary mismatch\n";
        return false;
    }
    return testAllInt16ValuesInBounds();
}

void populateChannelPlane(std::vector<float>& plane, int32_t channelIndex, int32_t numSamples) {
    plane.resize(static_cast<size_t>(numSamples));
    for (int32_t i = 0; i < numSamples; ++i) {
        const auto pcm = static_cast<int16_t>(((channelIndex + 1) * 4000) - (i % 1000));
        plane[static_cast<size_t>(i)] = openmedia::convertInt16ToFloat32(pcm);
    }
}

bool verifyChannelPlane(const std::vector<float>& plane, int32_t expectedSamples) {
    if (static_cast<int32_t>(plane.size()) != expectedSamples) {
        return false;
    }
    for (float val : plane) {
        if (!isSampleInUnitRange(val)) {
            return false;
        }
    }
    return true;
}

bool verifyAllPlanes(const openmedia::RawAudioBatch& batch, int32_t expectedChannels, int32_t expectedSamples) {
    if (batch.channels != expectedChannels) {
        return false;
    }
    if (static_cast<int32_t>(batch.channelData.size()) != expectedChannels) {
        return false;
    }
    for (int32_t ch = 0; ch < expectedChannels; ++ch) {
        if (!verifyChannelPlane(batch.channelData[static_cast<size_t>(ch)], expectedSamples)) {
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
        populateChannelPlane(batch.channelData[static_cast<size_t>(ch)], ch, kSamplesPerChannel);
    }
    if (!verifyAllPlanes(batch, kChannels51, kSamplesPerChannel)) {
        return false;
    }
    return batch.channelData[0][0] != batch.channelData[5][0];
}

openmedia::EncodedPacket makeSyntheticPcmPacket(int32_t channels, int32_t frames) {
    openmedia::EncodedPacket pkt;
    pkt.pts = 100;
    pkt.dts = 100;
    pkt.duration = frames;
    pkt.isKeyFrame = true;
    pkt.data.resize(static_cast<size_t>(channels * frames) * sizeof(int16_t));
    for (int32_t i = 0; i < frames * channels; ++i) {
        const auto val = static_cast<int16_t>((i % 2 == 0) ? 16384 : -16384);
        std::memcpy(pkt.data.data() + static_cast<size_t>(i) * sizeof(int16_t), &val, sizeof(int16_t));
    }
    return pkt;
}

bool isNotInitializedOnAllCalls(openmedia::CoreAudioDecoder& decoder,
                                const openmedia::EncodedPacket& pkt) {
    openmedia::RawAudioBatch batch;
    if (decoder.sendPacket(pkt) != openmedia::CoreStatus::NotInitialized) {
        return false;
    }
    if (decoder.decodeAudioBatch(64, batch) != openmedia::CoreStatus::NotInitialized) {
        return false;
    }
    return decoder.flush() == openmedia::CoreStatus::NotInitialized;
}

bool testDecoderValidationAndMovedGuards() {
    openmedia::CoreAudioDecoder decoder;
    openmedia::EncodedPacket pkt = makeSyntheticPcmPacket(2, 64);
    if (!isNotInitializedOnAllCalls(decoder, pkt)) {
        return false;
    }

    openmedia::StreamInfo badType;
    badType.type = openmedia::MediaType::Video;
    badType.codec = openmedia::CodecId::Opus;
    badType.channels = 2;
    if (decoder.initialize(badType, 48000) != openmedia::CoreStatus::InvalidArgument) {
        return false;
    }

    openmedia::StreamInfo zeroChannels;
    zeroChannels.type = openmedia::MediaType::Audio;
    zeroChannels.codec = openmedia::CodecId::Opus;
    zeroChannels.channels = 0;
    if (decoder.initialize(zeroChannels, 48000) != openmedia::CoreStatus::InvalidArgument) {
        return false;
    }

    openmedia::CoreAudioDecoder movedTo(std::move(decoder));
    decoder.reset();
    if (decoder.initialize(zeroChannels, 48000) != openmedia::CoreStatus::NotInitialized) {
        return false;
    }
    return isNotInitializedOnAllCalls(decoder, pkt);
}

bool verifyDecodedPcmBatch(openmedia::CoreAudioDecoder& decoder) {
    openmedia::EncodedPacket pkt = makeSyntheticPcmPacket(6, 128);
    if (decoder.sendPacket(pkt) != openmedia::CoreStatus::Ok) {
        return false;
    }
    openmedia::RawAudioBatch outBatch;
    if (decoder.decodeAudioBatch(128, outBatch) != openmedia::CoreStatus::Ok) {
        return false;
    }
    if (outBatch.numSamples <= 0) {
        return false;
    }
    return verifyAllPlanes(outBatch, 6, outBatch.numSamples);
}

bool testCoreAudioDecoderLifecycle() {
    if (!testDecoderValidationAndMovedGuards()) {
        return false;
    }

    openmedia::CoreAudioDecoder decoder;
    openmedia::StreamInfo pcmStream;
    pcmStream.type = openmedia::MediaType::Audio;
    pcmStream.codec = openmedia::CodecId::PCM;
    pcmStream.sampleRate = 48000;
    pcmStream.channels = 6;
    pcmStream.bitDepth = 16;

    if (decoder.initialize(pcmStream, 48000) != openmedia::CoreStatus::Ok) {
        return false;
    }
    if (!verifyDecodedPcmBatch(decoder) || decoder.flush() != openmedia::CoreStatus::Ok) {
        return false;
    }
    decoder.reset();
    openmedia::RawAudioBatch resetBatch;
    return decoder.decodeAudioBatch(64, resetBatch) == openmedia::CoreStatus::NeedMoreData;
}

} // namespace

int main() {
    if (!testPrecisionSnr() || !testClampingBounds() ||
        !testSurround51PlanarLayout() || !testCoreAudioDecoderLifecycle()) {
        std::cerr << "[TC-1.2.3] FAIL\n";
        return 1;
    }
    std::cout << "[TC-1.2.3] PASS: All CoreAudioDecoder unit checks succeeded.\n";
    return 0;
}
