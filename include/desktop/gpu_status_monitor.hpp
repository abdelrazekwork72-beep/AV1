#pragma once
/// @file gpu_status_monitor.hpp
/// @brief Live GPU/CPU stream decode telemetry viewer (VInfGpuStatusView parity).

#include "desktop/desktop_types.hpp"

#include <string>
#include <vector>

namespace openmedia::desktop {

class GpuStatusMonitor {
public:
    static std::vector<GpuStreamDisplayRow> formatRows(
        const std::vector<OpenMediaFrameDecodeTelemetry>& telemetry,
        UiLanguage lang);

    static std::vector<OpenMediaFrameDecodeTelemetry> parseTelemetryJson(const std::string& json);
    static std::vector<OpenMediaFrameDecodeTelemetry> loadFromFile(const std::string& jsonPath);
    static std::string localizePolicyReason(const std::string& rawReason, UiLanguage lang);
};

} // namespace openmedia::desktop
