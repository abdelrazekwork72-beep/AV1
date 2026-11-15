#include "exporter/chroma_fallback_guard.hpp"
#include <utility>

namespace openmedia::exporter {
namespace {

bool isHardware420Only(const std::string& enc) noexcept {
    return enc == "av1_nvenc" || enc == "av1_amf" || enc == "av1_qsv" || enc == "av1_vaapi";
}

void emitSessionWarningOnce(const std::string& requestedEncoder,
                            const std::string& fallbackCodec,
                            FallbackSessionState& sessionState,
                            const PrSDKErrorSuite* errorSuite,
                            bool reduceWarnings) {
    if (reduceWarnings && sessionState.warningEmitted) {
        ++sessionState.warningsSuppressed;
        return;
    }
    sessionState.warningEmitted = true;
    if (errorSuite != nullptr && errorSuite->SetEventString != nullptr) {
        const std::string detail = requestedEncoder + " failed; drained pending frames and fell back to " + fallbackCodec;
        errorSuite->SetEventString(1U, "Hardware Encoder Fallback", detail.c_str());
    }
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
    return selectCpuFallbackCodec(requestedEncoder);
}

std::string ChromaFallbackGuard::selectCpuFallbackCodec(const std::string& requestedEncoder) noexcept {
    if (requestedEncoder.find("vp9") != std::string::npos) {
        return "libvpx-vp9";
    }
    return "libsvtav1";
}

std::string ChromaFallbackGuard::drainBeforeFallback(const std::string& requestedEncoder,
                                                     std::vector<openmedia::RawVideoFrame>& pendingFrames,
                                                     std::vector<openmedia::RawVideoFrame>& outFlushedFrames,
                                                     FallbackSessionState& sessionState,
                                                     const PrSDKErrorSuite* errorSuite,
                                                     bool reduceWarnings) {
    outFlushedFrames.reserve(outFlushedFrames.size() + pendingFrames.size());
    for (auto& frame : pendingFrames) {
        outFlushedFrames.push_back(std::move(frame));
    }
    pendingFrames.clear();
    const std::string fallbackCodec = selectCpuFallbackCodec(requestedEncoder);
    emitSessionWarningOnce(requestedEncoder, fallbackCodec, sessionState, errorSuite, reduceWarnings);
    return fallbackCodec;
}

} // namespace openmedia::exporter
