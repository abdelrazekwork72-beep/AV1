#include "importer/audio_extractor.hpp"

#include <algorithm>

namespace openmedia::importer {

csSDK_int32 AudioExtractor::importAudio7(ImImportAudioRec7Ext* rec,
                                         const std::vector<int16_t>& interleavedPcm16,
                                         uint32_t channels) noexcept {
    if (rec == nullptr || rec->buffer == nullptr || channels == 0 || channels > 32U || rec->size <= 0) {
        return kImMemErr;
    }
    const size_t rawReq = static_cast<size_t>(rec->size);
    const size_t clampedReq = std::min(rawReq, kMaxAudioBatchSamples);
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
        for (size_t i = copyCount; i < clampedReq; ++i) {
            dst[i] = 0.0f;
        }
    }
    return kImNoErr;
}

} // namespace openmedia::importer
