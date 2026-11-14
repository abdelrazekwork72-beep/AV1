#include "exporter/vulkan_encoder.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace openmedia::exporter;
int main() {
    assert(std::strcmp(VulkanEncoder::selectBackend(true), "av1_vulkan") == 0);
    assert(std::strcmp(VulkanEncoder::selectBackend(false), "libsvtav1") == 0);
    std::cout << "[PASS] TC-3.3.4 VulkanEncoder av1_vulkan & fallback passed.\n";
    return 0;
}
