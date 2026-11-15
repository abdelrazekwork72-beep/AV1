#pragma once
/// @file chroma_fallback_guard.hpp
/// @brief Enforces AV1 Professional Profile 2 for 4:4:4 and graceful fallback with EOF drain & warning deduplication.
#include "exporter/exporter_types.hpp"
#include "openmedia/core_types.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace openmedia::exporter {

/// @brief Tracks per-session hardware fallback warning deduplication state.
struct FallbackSessionState {
    bool warningEmitted = false;
    uint32_t warningsSuppressed = 0;
};

class ChromaFallbackGuard {
public:
    static int32_t selectAv1Profile(int32_t chromaSubsampling, int32_t bitDepth = 8) noexcept;

    static std::string resolveWithFallback(const std::string& requestedEncoder,
                                           bool hwOpenSucceeded,
                                           bool& outErrorNotified,
                                           int32_t chromaSubsampling = 420,
                                           int32_t bitDepth = 8);

    /// @brief Select CPU fallback codec (libvpx-vp9 for VP9, libsvtav1 for AV1).
    static std::string selectCpuFallbackCodec(const std::string& requestedEncoder) noexcept;

    /// @brief Drain pending frames before falling back to CPU and emit at most one warning per session.
    static std::string drainBeforeFallback(const std::string& requestedEncoder,
                                           std::vector<openmedia::RawVideoFrame>& pendingFrames,
                                           std::vector<openmedia::RawVideoFrame>& outFlushedFrames,
                                           FallbackSessionState& sessionState,
                                           const PrSDKErrorSuite* errorSuite = nullptr,
                                           bool reduceWarnings = true);
};
} // namespace openmedia::exporter
