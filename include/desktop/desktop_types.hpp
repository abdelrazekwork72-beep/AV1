#pragma once
/// @file desktop_types.hpp
/// @brief Shared ABI-safe data types for OpenMediaDesktop.exe (Module 5).

#include "openmedia/core_types.hpp"
#include "importer/file_probe.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace openmedia::desktop {

/// @brief Supported UI localization languages.
enum class UiLanguage : uint8_t {
    English = 0, ///< English (LTR, "en")
    Arabic  = 1  ///< Arabic (RTL, "ar")
};

/// @brief Top-level import override policy matching Influx v1.7.1 + v2.0.0beta1.
enum class ImportOverridePolicy : uint8_t {
    ForceOverrideAllAdobe = 0,       ///< Force OpenMedia to override all Adobe importers
    NewFormatsCodecsAndFallback = 1, ///< Use for new formats, codecs and as a fallback (Default)
    NewFormatsAndFallbackOnly = 2,   ///< Use for new formats and as a fallback
    Disabled = 3                     ///< Disable OpenMedia Importer
};

/// @brief Formatted row for the GPU Status live telemetry table.
struct GpuStreamDisplayRow {
    uint64_t fileId = 0;
    std::string fileName;
    std::string hostApp;
    std::string codec;
    std::string resolution;
    std::string backendBadge;
    std::string statusSummary;
    bool isGpuActive = false;
};

/// @brief Summary returned by Cache & Diagnostic Log operations.
struct CachePurgeSummary {
    bool success = false;
    uint64_t filesRemoved = 0;
    uint64_t bytesRecovered = 0;
    std::string statusMessage;
};

/// @brief Plugin installation health item for MediaCore 7.0 verification.
struct PluginHealthItem {
    std::string componentName;
    std::string expectedPath;
    bool present = false;
    uint64_t sizeBytes = 0;
};

} // namespace openmedia::desktop
