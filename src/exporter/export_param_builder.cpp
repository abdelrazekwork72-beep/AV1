#include "exporter/export_param_builder.hpp"

#include <iostream>
#include <sstream>

namespace openmedia::exporter {
namespace {

std::string escapeJson(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (char ch : input) {
        if (ch == '"' || ch == '\\') {
            out.push_back('\\');
        }
        out.push_back(ch);
    }
    return out;
}

bool extractJsonString(const std::string& json, const std::string& key, std::string& outVal) {
    const std::string pattern = "\"" + key + "\":\"";
    const size_t start = json.find(pattern);
    if (start == std::string::npos) {
        return false;
    }
    const size_t valStart = start + pattern.size();
    size_t pos = valStart;
    while (pos < json.size()) {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            pos += 2;
            continue;
        }
        if (json[pos] == '"') {
            outVal = json.substr(valStart, pos - valStart);
            return true;
        }
        ++pos;
    }
    return false;
}

bool extractJsonInt(const std::string& json, const std::string& key, int32_t& outVal) {
    const std::string pattern = "\"" + key + "\":";
    const size_t start = json.find(pattern);
    if (start == std::string::npos) {
        return false;
    }
    const size_t valStart = start + pattern.size();
    const size_t valEnd = json.find_first_of(",}", valStart);
    try {
        outVal = std::stoi(json.substr(valStart, valEnd - valStart));
        return true;
    } catch (...) {
        return false;
    }
}

bool extractJsonBool(const std::string& json, const std::string& key, bool& outVal) {
    const std::string pattern = "\"" + key + "\":";
    const size_t start = json.find(pattern);
    if (start == std::string::npos) {
        return false;
    }
    const size_t valStart = start + pattern.size();
    outVal = (json.compare(valStart, 4, "true") == 0);
    return true;
}

void applyPresetLine(const std::string& k, const std::string& v, ExporterPresetParams& parsed, bool& sawVersion) {
    if (k == "version") {
        parsed.version = std::stoi(v);
        sawVersion = true;
    } else if (k == "width") {
        parsed.width = std::stoi(v);
    } else if (k == "height") {
        parsed.height = std::stoi(v);
    } else if (k == "crf") {
        parsed.crf = std::stoi(v);
    } else if (k == "codec") {
        parsed.codec = v;
    } else if (k == "color_metadata") {
        parsed.colorMetadata = ExportParamBuilder::parseColorMetadata(v);
    }
}

} // namespace

csSDK_int32 ExportParamBuilder::generateDefaults(ExporterPresetParams& outParams,
                                                 csSDK_int32* outSuiteVersion) noexcept {
    outParams.version = kOpenMediaExporterParamsVersion;
    outParams.width = 1920;
    outParams.height = 1080;
    outParams.crf = 28;
    outParams.codec = "libsvtav1";
    outParams.colorMetadata = ExportColorMetadata::BT709;
    if (outSuiteVersion != nullptr) {
        *outSuiteVersion = kOpenMediaExporterParamsVersion;
    }
    return kExNoErr;
}

const char* ExportParamBuilder::colorMetadataToString(ExportColorMetadata meta) noexcept {
    if (meta == ExportColorMetadata::BT2100_PQ) return "BT.2100_PQ";
    if (meta == ExportColorMetadata::BT2100_HLG) return "BT.2100_HLG";
    if (meta == ExportColorMetadata::Sony_SLog3) return "Sony_S-Log3";
    return "BT.709";
}

ExportColorMetadata ExportParamBuilder::parseColorMetadata(const std::string& value) noexcept {
    if (value == "BT.2100_PQ" || value == "BT.2100 PQ") return ExportColorMetadata::BT2100_PQ;
    if (value == "BT.2100_HLG" || value == "BT.2100 HLG") return ExportColorMetadata::BT2100_HLG;
    if (value == "Sony_S-Log3" || value == "S-Log3") return ExportColorMetadata::Sony_SLog3;
    return ExportColorMetadata::BT709;
}

std::string ExportParamBuilder::serializePreset(const ExporterPresetParams& params) {
    std::ostringstream oss;
    oss << "version=" << params.version << "\n"
        << "width=" << params.width << "\n"
        << "height=" << params.height << "\n"
        << "crf=" << params.crf << "\n"
        << "codec=" << params.codec << "\n"
        << "color_metadata=" << colorMetadataToString(params.colorMetadata) << "\n";
    return oss.str();
}

bool ExportParamBuilder::deserializePreset(const std::string& text, ExporterPresetParams& outParams) {
    std::istringstream iss(text);
    std::string line;
    ExporterPresetParams parsed{};
    bool sawVersion = false;
    while (std::getline(iss, line)) {
        const size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        applyPresetLine(line.substr(0, eq), line.substr(eq + 1), parsed, sawVersion);
    }
    if (!sawVersion || parsed.version != kOpenMediaExporterParamsVersion) {
        return false;
    }
    outParams = parsed;
    return true;
}

std::vector<std::string> PopupIpcBridge::buildLaunchArgs(uint64_t ownerHwnd) {
    return {"--popup", "--owner", std::to_string(ownerHwnd)};
}

std::string PopupIpcBridge::serializeJsonLine(const PopupIpcMessage& msg) {
    std::ostringstream oss;
    oss << kJsonPrefix << "{"
        << "\"config_toml\":\"" << escapeJson(msg.configTomlPath) << "\","
        << "\"gpu_status\":{"
        << "\"active_decoder\":\"" << escapeJson(msg.gpuStatus.activeDecoder) << "\","
        << "\"active_encoder\":\"" << escapeJson(msg.gpuStatus.activeEncoder) << "\","
        << "\"hardware_available\":" << (msg.gpuStatus.hardwareAvailable ? "true" : "false")
        << "},"
        << "\"preset\":{"
        << "\"version\":" << msg.preset.version << ","
        << "\"width\":" << msg.preset.width << ","
        << "\"height\":" << msg.preset.height << ","
        << "\"crf\":" << msg.preset.crf << ","
        << "\"codec\":\"" << escapeJson(msg.preset.codec) << "\","
        << "\"color_metadata\":\"" << ExportParamBuilder::colorMetadataToString(msg.preset.colorMetadata) << "\""
        << "}}\n";
    return oss.str();
}

bool PopupIpcBridge::parseJsonLine(const std::string& line, PopupIpcMessage& outMsg) {
    if (line.rfind(kJsonPrefix, 0) != 0) {
        return false;
    }
    const std::string payload = line.substr(5);
    PopupIpcMessage parsed{};
    extractJsonString(payload, "config_toml", parsed.configTomlPath);
    extractJsonString(payload, "active_decoder", parsed.gpuStatus.activeDecoder);
    extractJsonString(payload, "active_encoder", parsed.gpuStatus.activeEncoder);
    extractJsonBool(payload, "hardware_available", parsed.gpuStatus.hardwareAvailable);
    if (!extractJsonInt(payload, "version", parsed.preset.version) ||
        parsed.preset.version != kOpenMediaExporterParamsVersion) {
        return false;
    }
    extractJsonInt(payload, "width", parsed.preset.width);
    extractJsonInt(payload, "height", parsed.preset.height);
    extractJsonInt(payload, "crf", parsed.preset.crf);
    extractJsonString(payload, "codec", parsed.preset.codec);
    std::string colorMeta;
    if (extractJsonString(payload, "color_metadata", colorMeta)) {
        parsed.preset.colorMetadata = ExportParamBuilder::parseColorMetadata(colorMeta);
    }
    outMsg = parsed;
    return true;
}

bool PopupIpcBridge::exchangeOverStreams(std::istream& in,
                                         std::ostream& out,
                                         const PopupIpcMessage& request,
                                         PopupIpcMessage& outResponse) {
    out << serializeJsonLine(request);
    out.flush();
    std::string line;
    while (std::getline(in, line)) {
        if (parseJsonLine(line, outResponse)) {
            return true;
        }
    }
    return false;
}

} // namespace openmedia::exporter
