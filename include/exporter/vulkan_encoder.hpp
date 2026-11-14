#pragma once
/// @file vulkan_encoder.hpp
/// @brief FFmpeg 8.0 Vulkan Video av1_vulkan encoder wrapper.
#include "openmedia/core_types.hpp"
namespace openmedia::exporter {
/// @brief Vendor-neutral Vulkan Video AV1 encoder (VK_KHR_video_encode_av1).
class VulkanEncoder {
public:
    /// @brief Select av1_vulkan when Vulkan encode is available, else libsvtav1.
    static const char* selectBackend(bool vulkanEncodeAvailable) noexcept;
};
} // namespace openmedia::exporter
