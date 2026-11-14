#include "importer/vulkan_video_decoder.hpp"

namespace openmedia::importer {

bool VulkanVideoDecoder::supportsCodec(openmedia::CodecId codec) noexcept {
    return codec == openmedia::CodecId::AV1 || codec == openmedia::CodecId::VP9;
}

bool VulkanVideoDecoder::openSession(openmedia::CodecId codec, bool vulkanDeviceAvailable) noexcept {
    if (!supportsCodec(codec)) {
        open_ = false;
        hardwareActive_ = false;
        return false;
    }
    open_ = true;
    hardwareActive_ = vulkanDeviceAvailable;
    return true;
}

const char* VulkanVideoDecoder::backendName() const noexcept {
    if (!open_) {
        return "closed";
    }
    return hardwareActive_ ? "vulkan" : "cpu_fallback";
}

} // namespace openmedia::importer
