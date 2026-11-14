#pragma once
/// @file vulkan_video_decoder.hpp
/// @brief FFmpeg 8.0 Vulkan Video (VK_KHR_video_decode_av1 / vp9) wrapper with CPU fallback.

#include "openmedia/core_types.hpp"

namespace openmedia::importer {

/// @brief Cross-vendor Vulkan Video decoder supporting AV1 and VP9 with CPU fallback.
class VulkanVideoDecoder {
public:
    /// @brief Check whether codec is supported by Vulkan Video extensions.
    /// @param codec Codec identifier.
    /// @return true for AV1 and VP9.
    static bool supportsCodec(openmedia::CodecId codec) noexcept;

    /// @brief Open a Vulkan Video decode session, falling back to CPU if Vulkan is unavailable.
    /// @param codec Target video codec.
    /// @param vulkanDeviceAvailable Whether a Vulkan Video device is present.
    /// @return true if session opened (either Vulkan GPU or CPU fallback).
    bool openSession(openmedia::CodecId codec, bool vulkanDeviceAvailable) noexcept;

    /// @brief Query whether the active session uses Vulkan hardware acceleration.
    bool isHardwareActive() const noexcept { return hardwareActive_; }

    /// @brief Query human-readable active decoder backend name.
    const char* backendName() const noexcept;

private:
    bool open_ = false;
    bool hardwareActive_ = false;
};

} // namespace openmedia::importer
