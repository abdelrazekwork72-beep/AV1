#pragma once
/// @file export_param_builder.hpp
/// @brief Exporter parameter tree builder with SetParamsVersion(1001) and .epr preset I/O.

#include "exporter/exporter_types.hpp"

#include <cstdint>
#include <string>

namespace openmedia::exporter {

/// @brief Serializable exporter preset parameters.
struct ExporterPresetParams {
    csSDK_int32 version = kOpenMediaExporterParamsVersion;
    int32_t width = 1920;
    int32_t height = 1080;
    int32_t crf = 28;
    std::string codec = "libsvtav1";
};

/// @brief Builds default Adobe parameter trees and serializes .epr presets.
class ExportParamBuilder {
public:
    /// @brief Generate default parameters and invoke SetParamsVersion(1001).
    static csSDK_int32 generateDefaults(ExporterPresetParams& outParams,
                                        csSDK_int32* outSuiteVersion = nullptr) noexcept;

    /// @brief Serialize parameters to a deterministic key=value .epr preset string.
    static std::string serializePreset(const ExporterPresetParams& params);

    /// @brief Deserialize .epr preset string, rejecting stale versions if invalid.
    static bool deserializePreset(const std::string& text, ExporterPresetParams& outParams);
};

} // namespace openmedia::exporter
