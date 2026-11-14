#include "exporter/export_param_builder.hpp"

#include <sstream>

namespace openmedia::exporter {

csSDK_int32 ExportParamBuilder::generateDefaults(ExporterPresetParams& outParams,
                                                 csSDK_int32* outSuiteVersion) noexcept {
    outParams.version = kOpenMediaExporterParamsVersion;
    outParams.width = 1920;
    outParams.height = 1080;
    outParams.crf = 28;
    outParams.codec = "libsvtav1";
    if (outSuiteVersion != nullptr) {
        *outSuiteVersion = kOpenMediaExporterParamsVersion;
    }
    return kExNoErr;
}

std::string ExportParamBuilder::serializePreset(const ExporterPresetParams& params) {
    std::ostringstream oss;
    oss << "version=" << params.version << "\n"
        << "width=" << params.width << "\n"
        << "height=" << params.height << "\n"
        << "crf=" << params.crf << "\n"
        << "codec=" << params.codec << "\n";
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
        const std::string k = line.substr(0, eq);
        const std::string v = line.substr(eq + 1);
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
        }
    }
    if (!sawVersion || parsed.version != kOpenMediaExporterParamsVersion) {
        return false;
    }
    outParams = parsed;
    return true;
}

} // namespace openmedia::exporter
