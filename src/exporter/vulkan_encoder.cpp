#include "exporter/vulkan_encoder.hpp"
namespace openmedia::exporter {
const char* VulkanEncoder::selectBackend(bool vulkanEncodeAvailable) noexcept {
    return vulkanEncodeAvailable ? "av1_vulkan" : "libsvtav1";
}
} // namespace openmedia::exporter
