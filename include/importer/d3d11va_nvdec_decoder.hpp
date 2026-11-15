#pragma once
/// @file d3d11va_nvdec_decoder.hpp
/// @brief Hardware video decoder wrapper for D3D11VA/NVDEC with smart decode policy & 4-stream cap.

#include "importer/file_probe.hpp"
#include "openmedia/core_types.hpp"

#include <atomic>
#include <cstdint>
#include <vector>

namespace openmedia::importer {

/// @brief Hardware acceleration backend selection.
enum class HwBackend {
    None,
    D3D11VA,
    NVDEC,
    SoftwareFallback
};

/// @brief Policy decision returned by evaluateDecodePolicy().
enum class DecodePolicyDecision {
    UseGpu,
    UseCpu
};

/// @brief Wraps D3D11VA/NVDEC decoding with 4-stream atomic cap, resolution threshold, and CPU fallback.
class D3D11VaNvdecDecoder {
public:
    /// @brief Process-wide cap on simultaneous active GPU decode streams.
    static constexpr uint32_t kMaxConcurrentGpuDecoders = 4U;

    /// @brief Minimum pixel count (2560x1600 = 4,096,000) required in Recommended mode.
    static constexpr uint64_t kRecommendedMinGpuPixels = 2560ULL * 1600ULL;

    ~D3D11VaNvdecDecoder();

    /// @brief Evaluates whether a stream should use GPU or CPU based on resolution, mode, and 4-stream cap.
    static DecodePolicyDecision evaluateDecodePolicy(uint32_t width,
                                                     uint32_t height,
                                                     GpuDecodeMode mode) noexcept;

    /// @brief Returns the current process-wide active GPU decode stream count.
    static uint32_t activeGpuStreamCount() noexcept;

    /// @brief Resets process-wide GPU stream counter and session warning flag (for unit tests).
    static void resetGlobalGpuStateForTesting() noexcept;

    /// @brief Initialize hardware backend, falling back to CPU on failure or cap exhaustion.
    HwBackend initialize(HwBackend preferred, bool simulateHwFailure = false) noexcept;

    /// @brief Initialize using the smart decode policy engine (resolution + Config mode + 4-stream cap).
    HwBackend initializeWithPolicy(HwBackend preferred,
                                   uint32_t width,
                                   uint32_t height,
                                   GpuDecodeMode mode,
                                   bool simulateHwFailure = false) noexcept;

    /// @brief Cleanly drains GPU frames, releases the GPU slot, and demotes to CPU with once-per-session warning.
    bool drainBeforeFallback(bool simulateError = false) noexcept;

    /// @brief Releases any held GPU stream slot.
    void releaseSlot() noexcept;

    /// @brief Transfer a 10-bit P010LE or 8-bit NV12 hardware surface to 16-byte aligned buffer.
    bool transferAlignedSurface(uint32_t width,
                                uint32_t height,
                                bool is10Bit,
                                std::vector<uint8_t>& outBuffer,
                                uint32_t& outStride) const;

    HwBackend activeBackend() const noexcept { return active_; }
    bool holdsGpuSlot() const noexcept { return holdsGpuSlot_; }
    static bool hasLoggedFallbackWarning() noexcept;

private:
    bool tryAcquireGpuSlot() noexcept;

    HwBackend active_ = HwBackend::None;
    bool holdsGpuSlot_ = false;
};

} // namespace openmedia::importer
