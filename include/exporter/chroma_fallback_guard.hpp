#pragma once
/// @file chroma_fallback_guard.hpp
/// @brief Enforces AV1 Professional Profile 2 for 4:4:4 and graceful fallback to libsvtav1.
#include "exporter/exporter_types.hpp"
#include <cstdint>
#include <string>
namespace openmedia::exporter {
class ChromaFallbackGuard {
public:
    static int32_t selectAv1Profile(int32_t chromaSubsampling, int32_t bitDepth = 8) noexcept;
    static std::string resolveWithFallback(const std::string& requestedEncoder,
                                           bool hwOpenSucceeded,
                                           bool& outErrorNotified,
                                           int32_t chromaSubsampling = 420,
                                           int32_t bitDepth = 8);
};
} // namespace openmedia::exporter
