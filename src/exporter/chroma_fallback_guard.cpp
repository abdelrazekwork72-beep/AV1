#include "exporter/chroma_fallback_guard.hpp"
namespace openmedia::exporter {
namespace {
bool isHardware420Only(const std::string& enc) noexcept {
    return enc == "av1_nvenc" || enc == "av1_amf" || enc == "av1_qsv" || enc == "av1_vaapi";
}
} // namespace
int32_t ChromaFallbackGuard::selectAv1Profile(int32_t chromaSubsampling, int32_t bitDepth) noexcept {
    if (chromaSubsampling == 444 || chromaSubsampling == 422 || bitDepth == 12) return 2;
    return 0;
}
std::string ChromaFallbackGuard::resolveWithFallback(const std::string& requestedEncoder,
                                                     bool hwOpenSucceeded,
                                                     bool& outErrorNotified,
                                                     int32_t chromaSubsampling,
                                                     int32_t bitDepth) {
    const bool unsupportedChromaOnHw =
        isHardware420Only(requestedEncoder) && (chromaSubsampling == 444 || bitDepth == 12);
    if (hwOpenSucceeded && !unsupportedChromaOnHw) {
        outErrorNotified = false;
        return requestedEncoder;
    }
    outErrorNotified = true;
    return "libsvtav1";
}
} // namespace openmedia::exporter
