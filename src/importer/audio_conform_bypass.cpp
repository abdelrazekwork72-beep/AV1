#include "importer/audio_conform_bypass.hpp"

namespace openmedia::importer {

csSDK_int32 AudioConformBypass::configureInit(ImImportInfoRecExt* info) noexcept {
    if (info == nullptr) {
        return kImMemErr;
    }
    info->avoidAudioConform = kPrTrueVal;
    info->canProvidePeakAudio = kPrTrueVal;
    return kImNoErr;
}

csSDK_int32 AudioConformBypass::configureFileInfo(ImFileInfoRec8Ext* fileInfo) noexcept {
    if (fileInfo == nullptr) {
        return kImMemErr;
    }
    fileInfo->accessModes |= kRandomAccessImportFlag;
    return kImNoErr;
}

int64_t AudioConformBypass::sampleToByteOffset(int64_t sampleIndex,
                                               uint32_t sampleRate,
                                               uint32_t avgBitrateBps) noexcept {
    if (sampleIndex <= 0 || sampleRate == 0 || avgBitrateBps == 0) {
        return 0;
    }
    return (sampleIndex * static_cast<int64_t>(avgBitrateBps / 8U)) / static_cast<int64_t>(sampleRate);
}

} // namespace openmedia::importer
