#include "exporter/cpu_codecs.hpp"
namespace openmedia::exporter {
const char* CpuCodecBackends::selectEncoderName(CodecId codec, bool preferSpeed) noexcept {
    if (codec == CodecId::AV1) return preferSpeed ? "libsvtav1" : "libaom-av1";
    if (codec == CodecId::VP9) return "libvpx-vp9";
    if (codec == CodecId::Opus) return "libopus";
    if (codec == CodecId::FLAC) return "libflac";
    return "unknown";
}
bool CpuCodecBackends::supportsFormat(CodecId codec, int32_t bitDepth, int32_t chroma) noexcept {
    if (bitDepth != 8 && bitDepth != 10 && bitDepth != 12) return false;
    if (chroma != 420 && chroma != 422 && chroma != 444) return false;
    return codec == CodecId::AV1 || codec == CodecId::VP9;
}
} // namespace openmedia::exporter
