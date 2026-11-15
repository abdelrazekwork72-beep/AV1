/// @file audio_extractor.cpp
/// @brief Implementation of AudioExtractor with fault-tolerant silence fallback and handle cleanup.

#include "importer/audio_extractor.hpp"

#include <algorithm>

namespace openmedia::importer {
namespace {

csSDK_int32 fillSilence(ImImportAudioRec7Ext* rec, uint32_t channels, size_t sampleCount) noexcept {
    for (uint32_t ch = 0; ch < channels; ++ch) {
        if (rec->buffer[ch] == nullptr) {
            return kImMemErr;
        }
        std::fill_n(rec->buffer[ch], sampleCount, 0.0f);
    }
    return kImNoErr;
}

} // namespace

csSDK_int32 AudioExtractor::importAudio7(ImImportAudioRec7Ext* rec,
                                         const std::vector<int16_t>& interleavedPcm16,
                                         uint32_t channels) noexcept {
    if (rec == nullptr || rec->buffer == nullptr || channels == 0 || channels > 32U || rec->size == 0) {
        return kImMemErr;
    }
    const size_t clampedReq = std::min(static_cast<size_t>(rec->size), kMaxAudioBatchSamples);
    const size_t availSamples = interleavedPcm16.size() / static_cast<size_t>(channels);
    const size_t copyCount = std::min(clampedReq, availSamples);
    constexpr float kInvFullScale = 1.0f / 32767.0f;

    for (uint32_t ch = 0; ch < channels; ++ch) {
        if (rec->buffer[ch] == nullptr) {
            return kImMemErr;
        }
        float* dst = rec->buffer[ch];
        for (size_t i = 0; i < copyCount; ++i) {
            const float v = static_cast<float>(interleavedPcm16[i * channels + ch]) * kInvFullScale;
            dst[i] = std::clamp(v, -1.0f, 1.0f);
        }
        std::fill(dst + copyCount, dst + clampedReq, 0.0f);
    }
    return kImNoErr;
}

csSDK_int32 AudioExtractor::importAudio7FaultTolerant(ImImportAudioRec7Ext* rec,
                                                      const std::vector<int16_t>& interleavedPcm16,
                                                      uint32_t channels,
                                                      bool packetCorrupted) noexcept {
    if (rec == nullptr || rec->buffer == nullptr || channels == 0 || channels > 32U || rec->size == 0) {
        return kImMemErr;
    }
    const size_t clampedReq = std::min(static_cast<size_t>(rec->size), kMaxAudioBatchSamples);
    if (packetCorrupted || interleavedPcm16.size() < static_cast<size_t>(channels)) {
        return fillSilence(rec, channels, clampedReq);
    }
    return importAudio7(rec, interleavedPcm16, channels);
}

csSDK_int32 AudioExtractor::initializeAudioWithCleanup(ICoreDemuxer* demuxer,
                                                       uint32_t channels,
                                                       bool simulateInitFailure) noexcept {
    if (simulateInitFailure || channels == 0 || channels > 32U) {
        if (demuxer != nullptr) {
            demuxer->close();
        }
        return kImBadFile;
    }
    return kImNoErr;
}

} // namespace openmedia::importer
