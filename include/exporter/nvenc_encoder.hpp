#pragma once
/// @file nvenc_encoder.hpp
/// @brief NVIDIA NVENC (av1_nvenc / hevc_nvenc) wrapper enforcing driver >= 570.0 and CWE-427 DLL safety.

#include <cstdint>
#include <string>
#include <vector>

namespace openmedia::exporter {

/// @brief Validates NVIDIA driver >= 570.0, secure SYSTEM32 DLL loading, and av1_nvenc options.
class NvencEncoder {
public:
    static constexpr double kMinDriverVersion = 570.0;
    static constexpr uint32_t kLoadLibrarySearchSystem32 = 0x00000800U;
    static bool isDriverSupported(double driverVersion) noexcept;
    static bool validateSecureLibraryLoad(const std::string& dllName, uint32_t loadFlags) noexcept;
    static std::vector<std::string> filterAv1Options(const std::vector<std::string>& opts);
};

} // namespace openmedia::exporter
