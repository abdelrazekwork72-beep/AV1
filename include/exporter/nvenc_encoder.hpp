#pragma once
/// @file nvenc_encoder.hpp
/// @brief NVIDIA NVENC (av1_nvenc / hevc_nvenc) wrapper enforcing driver >= 570.0, CWE-427 DLL safety, and mid-render drain.

#include "exporter/chroma_fallback_guard.hpp"
#include "openmedia/core_types.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace openmedia::exporter {

/// @brief Validates NVIDIA driver >= 570.0, secure SYSTEM32 DLL loading, av1_nvenc options, and mid-render fallback.
class NvencEncoder {
public:
    static constexpr double kMinDriverVersion = 570.0;
    static constexpr uint32_t kLoadLibrarySearchSystem32 = 0x00000800U;
    static bool isDriverSupported(double driverVersion) noexcept;
    static bool validateSecureLibraryLoad(const std::string& dllName, uint32_t loadFlags) noexcept;
    static std::vector<std::string> filterAv1Options(const std::vector<std::string>& opts);
    static bool shouldFallbackMidRender(int32_t nvencStatusCode, bool deviceLost) noexcept;
    static std::string handleMidRenderFailure(std::vector<openmedia::RawVideoFrame>& pendingFrames,
                                              std::vector<openmedia::RawVideoFrame>& outFlushedFrames,
                                              FallbackSessionState& sessionState,
                                              const PrSDKErrorSuite* errorSuite = nullptr,
                                              bool reduceWarnings = true);
};

} // namespace openmedia::exporter
