#include "exporter/nvenc_encoder.hpp"

namespace openmedia::exporter {

bool NvencEncoder::isDriverSupported(double driverVersion) noexcept {
    return driverVersion >= kMinDriverVersion;
}

bool NvencEncoder::validateSecureLibraryLoad(const std::string& dllName, uint32_t loadFlags) noexcept {
    if ((loadFlags & kLoadLibrarySearchSystem32) == 0U) {
        return false;
    }
    if (dllName.find('/') != std::string::npos ||
        dllName.find('\\') != std::string::npos ||
        dllName.find("..") != std::string::npos) {
        return false;
    }
    return dllName == "nvEncodeAPI64.dll" || dllName == "libnvidia-encode.so.1";
}

std::vector<std::string> NvencEncoder::filterAv1Options(const std::vector<std::string>& opts) {
    std::vector<std::string> out;
    out.reserve(opts.size());
    for (const auto& o : opts) {
        if (o.find("qp_cr_offset") == std::string::npos) {
            out.push_back(o);
        }
    }
    return out;
}

bool NvencEncoder::shouldFallbackMidRender(int32_t nvencStatusCode, bool deviceLost) noexcept {
    return deviceLost || nvencStatusCode != 0;
}

std::string NvencEncoder::handleMidRenderFailure(std::vector<openmedia::RawVideoFrame>& pendingFrames,
                                                 std::vector<openmedia::RawVideoFrame>& outFlushedFrames,
                                                 FallbackSessionState& sessionState,
                                                 const PrSDKErrorSuite* errorSuite,
                                                 bool reduceWarnings) {
    return ChromaFallbackGuard::drainBeforeFallback(
        "av1_nvenc", pendingFrames, outFlushedFrames, sessionState, errorSuite, reduceWarnings);
}

} // namespace openmedia::exporter
