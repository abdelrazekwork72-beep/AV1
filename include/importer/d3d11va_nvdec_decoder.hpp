#pragma once
/// @file d3d11va_nvdec_decoder.hpp
/// @brief Hardware video decoder wrapper for D3D11VA / NVDEC with CPU fallback.

#include "openmedia/core_types.hpp"

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

/// @brief Wraps D3D11VA/NVDEC decoding with 16-byte aligned system memory transfer and CPU fallback.
class D3D11VaNvdecDecoder {
public:
    /// @brief Initialize hardware backend, falling back to CPU on failure.
    /// @param preferred Preferred hardware backend.
    /// @param simulateHwFailure Force hardware initialization failure for testing fallback.
    /// @return Active backend selected.
    HwBackend initialize(HwBackend preferred, bool simulateHwFailure = false) noexcept;

    /// @brief Transfer a 10-bit P010LE or 8-bit NV12 hardware surface to 16-byte aligned buffer.
    /// @param width Frame width in pixels.
    /// @param height Frame height in pixels.
    /// @param is10Bit True for P010LE (2 bytes/sample), false for NV12 (1 byte/sample).
    /// @param outBuffer Receives 16-byte aligned system-memory buffer.
    /// @param outStride Receives 16-byte aligned row stride in bytes.
    /// @return true on success, false on invalid dimensions.
    bool transferAlignedSurface(uint32_t width,
                                uint32_t height,
                                bool is10Bit,
                                std::vector<uint8_t>& outBuffer,
                                uint32_t& outStride) const;

    /// @brief Query currently active backend.
    HwBackend activeBackend() const noexcept { return active_; }

private:
    HwBackend active_ = HwBackend::None;
};

} // namespace openmedia::importer
