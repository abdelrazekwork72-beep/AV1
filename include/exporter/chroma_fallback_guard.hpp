#pragma once
/// @file chroma_fallback_guard.hpp
/// @brief Enforces AV1 Professional Profile 2 for 4:4:4 and graceful fallback to libsvtav1.
#include "exporter/exporter_types.hpp"
#include <string>
namespace openmedia::exporter {
/// @brief Guards chroma profile selection and catches hw open failures via PrSDKErrorSuite.
class ChromaFallbackGuard {
public:
    /// @brief Return AV1 profile index (2 for 4:4:4, 0 for 4:2:0).
    static int32_t selectAv1Profile(int32_t chromaSubsampling) noexcept;
    /// @brief Fall back to libsvtav1 when hardware avcodec_open2 fails.
    static std::string resolveWithFallback(const std::string& requestedEncoder,
                                           bool hwOpenSucceeded,
                                           bool& outErrorNotified);
};
} // namespace openmedia::exporter
