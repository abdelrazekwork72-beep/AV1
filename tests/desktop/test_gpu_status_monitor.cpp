#include "desktop/gpu_status_monitor.hpp"
#include <cassert>
#include <iostream>

int main() {
    using namespace openmedia::desktop;

    const std::string json = R"JSON([
        {"fileId":1,"filePath":"C:\\Media\\interview.mkv","hostAppName":"Premiere Pro","codecName":"av1","width":3840,"height":2160,"activeBackend":"NVDEC (CUDA)","decodedOnGpu":true,"policyReason":"GPU active"},
        {"fileId":2,"filePath":"C:\\Media\\broll.webm","hostAppName":"Media Encoder","codecName":"vp9","width":1920,"height":1080,"activeBackend":"CPU","decodedOnGpu":false,"policyReason":"Clip below 2560x1600 GPU threshold"}
    ])JSON";

    auto telemetry = GpuStatusMonitor::parseTelemetryJson(json);
    assert(telemetry.size() == 2);
    assert(telemetry[0].filePath.find("interview.mkv") != std::string::npos);
    assert(telemetry[0].decodedOnGpu == true);
    assert(telemetry[1].decodedOnGpu == false);

    auto enRows = GpuStatusMonitor::formatRows(telemetry, UiLanguage::English);
    assert(enRows.size() == 2);
    assert(enRows[0].fileName == "interview.mkv");
    assert(enRows[0].isGpuActive == true);

    auto arRows = GpuStatusMonitor::formatRows(telemetry, UiLanguage::Arabic);
    assert(arRows.size() == 2);
    assert(arRows[1].statusSummary != enRows[1].statusSummary);

    std::cout << "test_gpu_status_monitor: PASSED\n";
    return 0;
}
