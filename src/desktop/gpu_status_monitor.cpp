#include "desktop/gpu_status_monitor.hpp"

#include <fstream>
#include <sstream>

namespace openmedia::desktop {
namespace {

std::string extractJsonField(const std::string& obj, const std::string& key) {
    const std::string pat = "\"" + key + "\":";
    const size_t pos = obj.find(pat);
    if (pos == std::string::npos) return "";
    size_t valStart = obj.find_first_not_of(" \t\r\n", pos + pat.size());
    if (valStart == std::string::npos) return "";
    if (obj[valStart] == '"') {
        size_t end = valStart + 1U;
        while (end < obj.size()) {
            if (obj[end] == '\\' && end + 1U < obj.size()) {
                end += 2U;
                continue;
            }
            if (obj[end] == '"') break;
            ++end;
        }
        return obj.substr(valStart + 1U, end - valStart - 1U);
    }
    const size_t end = obj.find_first_of(",}\r\n", valStart);
    return obj.substr(valStart, end - valStart);
}

std::string fileNameOnly(const std::string& path) {
    const size_t pos = path.find_last_of("\\/");
    return (pos == std::string::npos) ? path : path.substr(pos + 1U);
}

} // namespace

std::string GpuStatusMonitor::localizePolicyReason(const std::string& rawReason, UiLanguage lang) {
    if (lang != UiLanguage::Arabic) return rawReason;
    if (rawReason.find("2560x1600") != std::string::npos) {
        return "الدقة أقل من الحد الموصى به (2560x1600) — يعمل على المعالج CPU";
    }
    if (rawReason.find("cap") != std::string::npos || rawReason.find("4") != std::string::npos) {
        return "تم الوصول للحد الأقصى (4 مسارات GPU متزامنة) — تحويل تلقائي لـ CPU";
    }
    if (rawReason.find("disabled") != std::string::npos) {
        return "تسريع GPU معطل من الإعدادات";
    }
    return rawReason.empty() ? "فك تشفير نشط" : rawReason;
}

std::vector<GpuStreamDisplayRow> GpuStatusMonitor::formatRows(
    const std::vector<OpenMediaFrameDecodeTelemetry>& telemetry,
    UiLanguage lang) {
    std::vector<GpuStreamDisplayRow> rows;
    rows.reserve(telemetry.size());
    for (const auto& t : telemetry) {
        GpuStreamDisplayRow r;
        r.fileId = t.fileId;
        r.fileName = fileNameOnly(t.filePath);
        r.hostApp = t.hostAppName.empty() ? "Premiere Pro" : t.hostAppName;
        r.codec = t.codecName;
        r.resolution = std::to_string(t.width) + "x" + std::to_string(t.height);
        r.isGpuActive = t.decodedOnGpu;
        r.backendBadge = t.decodedOnGpu
                             ? ((lang == UiLanguage::Arabic ? "GPU نشط: " : "GPU: ") + t.activeBackend)
                             : (lang == UiLanguage::Arabic ? "CPU (معالج)" : "CPU (Software)");
        r.statusSummary = localizePolicyReason(t.policyReason, lang);
        rows.push_back(std::move(r));
    }
    return rows;
}

std::vector<OpenMediaFrameDecodeTelemetry> GpuStatusMonitor::parseTelemetryJson(const std::string& json) {
    std::vector<OpenMediaFrameDecodeTelemetry> out;
    size_t pos = 0;
    while ((pos = json.find('{', pos)) != std::string::npos) {
        const size_t end = json.find('}', pos);
        if (end == std::string::npos) break;
        const std::string obj = json.substr(pos, end - pos + 1U);
        pos = end + 1U;
        const std::string fp = extractJsonField(obj, "filePath");
        if (fp.empty()) continue;
        OpenMediaFrameDecodeTelemetry entry;
        entry.filePath = fp;
        entry.hostAppName = extractJsonField(obj, "hostAppName");
        entry.codecName = extractJsonField(obj, "codecName");
        entry.pixelFormat = extractJsonField(obj, "pixelFormat");
        entry.colorSpace = extractJsonField(obj, "colorSpace");
        entry.activeBackend = extractJsonField(obj, "activeBackend");
        entry.policyReason = extractJsonField(obj, "policyReason");
        entry.decodedOnGpu = (extractJsonField(obj, "decodedOnGpu") == "true");
        try { entry.fileId = std::stoull(extractJsonField(obj, "fileId")); } catch (...) {}
        try { entry.width = static_cast<uint32_t>(std::stoul(extractJsonField(obj, "width"))); } catch (...) {}
        try { entry.height = static_cast<uint32_t>(std::stoul(extractJsonField(obj, "height"))); } catch (...) {}
        out.push_back(std::move(entry));
    }
    return out;
}

std::vector<OpenMediaFrameDecodeTelemetry> GpuStatusMonitor::loadFromFile(const std::string& jsonPath) {
    std::ifstream ifs(jsonPath);
    if (!ifs.is_open()) return {};
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return parseTelemetryJson(oss.str());
}

} // namespace openmedia::desktop
