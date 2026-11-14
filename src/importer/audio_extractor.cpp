#include "importer/audio_extractor.hpp"

#include <algorithm>

namespace openmedia::importer {

csSDK_int32 AudioExtractor::importAudio7(ImImportAudioRec7Ext* rec,
                                         const std::vector<int16_t>& interleavedPcm16,
                                         uint32_t channels) noexcept {
    if (rec == nullptr || rec->buffer == nullptr || channels == 0 || channels > 32U || rec->size <= 0) {
        return kImMemErr;
    }
    const size_t requestedSamples = static_cast<size_t>(rec->size);
    const size_t availableSamples = interleavedPcm16.size() / channels;
    const size_t count = std::min(requestedSamples, availableSamples);
    constexpr float kScale = 1.0f / 32768.0f;

    for (uint32_t ch = 0; ch < channels; ++ch) {
        if (rec->buffer[ch] == nullptr) {
            return kImMemErr;
        }
        float* dst = rec->buffer[ch];
        for (size_t i = 0; i < count; ++i) {
            dst[i] = static_cast<float>(interleavedPcm16[i * channels + ch]) * kScale;
        }
    }
    return kImNoErr;
}

} // namespace openmedia::importer
