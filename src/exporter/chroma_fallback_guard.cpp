#include "exporter/chroma_fallback_guard.hpp"
namespace openmedia::exporter {
int32_t ChromaFallbackGuard::selectAv1Profile(int32_t chromaSubsampling) noexcept {
    if (chromaSubsampling == 444) return 2;
    if (chromaSubsampling == 422) return 2;
    return 0;
}
std::string ChromaFallbackGuard::resolveWithFallback(const std::string& requestedEncoder,
                                                     bool hwOpenSucceeded,
                                                     bool& outErrorNotified) {
    if (hwOpenSucceeded) {
        outErrorNotified = false;
        return requestedEncoder;
    }
    outErrorNotified = true;
    return "libsvtav1";
}
} // namespace openmedia::exporter
