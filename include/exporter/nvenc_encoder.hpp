#pragma once
/// @file nvenc_encoder.hpp
/// @brief NVIDIA NVENC (av1_nvenc / hevc_nvenc) wrapper enforcing driver >= 570.0.
#include <string>
#include <vector>
namespace openmedia::exporter {
/// @brief Validates NVIDIA driver >= 570.0 and filters unsupported av1_nvenc options.
class NvencEncoder {
public:
    /// @brief Minimum required NVIDIA driver version for AV1 NVENC stability.
    static constexpr double kMinDriverVersion = 570.0;
    /// @brief Check whether driverVersion >= 570.0.
    static bool isDriverSupported(double driverVersion) noexcept;
    /// @brief Filter out unsupported options (such as qp_cr_offset) for av1_nvenc.
    static std::vector<std::string> filterAv1Options(const std::vector<std::string>& opts);
};
} // namespace openmedia::exporter
