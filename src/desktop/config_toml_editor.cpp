#include "desktop/config_toml_editor.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace openmedia::desktop {
namespace {

constexpr size_t kMaxConfigBytes = 1024U * 1024U;

std::string toLowerTrim(std::string s) {
    const size_t first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const size_t last = s.find_last_not_of(" \t\r\n");
    s = s.substr(first, last - first + 1U);
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return s;
}

const char* gpuModeStr(importer::GpuDecodeMode mode) noexcept {
    if (mode == importer::GpuDecodeMode::Available) return "available";
    if (mode == importer::GpuDecodeMode::Disabled) return "disabled";
    return "recommended";
}

const char* thumbModeStr(importer::ThumbnailTimecodeMode mode) noexcept {
    if (mode == importer::ThumbnailTimecodeMode::HideWhenZero) return "hide_when_zero";
    if (mode == importer::ThumbnailTimecodeMode::AlwaysHide) return "always_hide";
    return "always_show";
}

const char* durationModeStr(importer::DurationCalculationMode mode) noexcept {
    return (mode == importer::DurationCalculationMode::FrameTimestamp) ? "frame_timestamp" : "metadata";
}

} // namespace

std::vector<std::string> ConfigTomlEditor::normalizeExtensions(const std::string& commaSeparated) {
    std::vector<std::string> out;
    std::istringstream iss(commaSeparated);
    std::string token;
    while (std::getline(iss, token, ',')) {
        std::string clean = toLowerTrim(token);
        while (!clean.empty() && clean.front() == '.') {
            clean.erase(clean.begin());
        }
        if (!clean.empty() && std::find(out.begin(), out.end(), clean) == out.end()) {
            out.push_back(clean);
        }
    }
    return out;
}

std::string ConfigTomlEditor::formatExtensions(const std::vector<std::string>& exts) {
    std::string out;
    for (size_t i = 0; i < exts.size(); ++i) {
        if (i > 0) out += ", ";
        out += exts[i];
    }
    return out;
}

importer::OpenMediaConfig ConfigTomlEditor::clampDefaults(importer::OpenMediaConfig cfg) {
    cfg.maxFrameCacheSizeMB = std::clamp<uint32_t>(cfg.maxFrameCacheSizeMB, 32U, 8192U);
    cfg.maxPercentUsageAvailableMemory = std::clamp<uint32_t>(cfg.maxPercentUsageAvailableMemory, 10U, 90U);
    if (cfg.maxLogFileSizeMB < -1 || cfg.maxLogFileSizeMB == 0) cfg.maxLogFileSizeMB = 25;
    if (cfg.uiLanguage != "ar") cfg.uiLanguage = "en";
    return cfg;
}

importer::OpenMediaConfig ConfigTomlEditor::factoryDefaults() {
    importer::OpenMediaConfig cfg;
    cfg.overrideExtensions = {"mkv", "webm"};
    return clampDefaults(cfg);
}

importer::OpenMediaConfig ConfigTomlEditor::loadFromFile(const std::string& path, bool* outRecoveredMalformed) {
    if (outRecoveredMalformed) *outRecoveredMalformed = false;
    std::ifstream ifs(path, std::ios::ate | std::ios::binary);
    if (!ifs.is_open()) {
        return factoryDefaults();
    }
    const auto sz = ifs.tellg();
    if (sz <= 0 || static_cast<size_t>(sz) > kMaxConfigBytes) {
        if (outRecoveredMalformed) *outRecoveredMalformed = true;
        return factoryDefaults();
    }
    ifs.seekg(0);
    std::string content(static_cast<size_t>(sz), '\0');
    ifs.read(&content[0], sz);
    return clampDefaults(importer::OpenMediaConfig::parseConfigToml(content));
}

std::string ConfigTomlEditor::serializeToToml(const importer::OpenMediaConfig& rawCfg) {
    const auto cfg = clampDefaults(rawCfg);
    std::ostringstream oss;
    oss << "# OpenMedia Control Center Config.toml (Influx v1.7.1 + v2.0.0 Parity)\n"
        << "uiLanguage = \"" << cfg.uiLanguage << "\"\n"
        << "importPolicy = \"" << cfg.importPolicy << "\"\n"
        << "disable = " << (cfg.disable ? "true" : "false") << "\n"
        << "overrideAdobe = " << (cfg.overrideAdobe ? "true" : "false") << "\n"
        << "disableAutomaticOverride = " << (cfg.disableAutomaticOverride ? "true" : "false") << "\n"
        << "overrideExtensions = [";
    for (size_t i = 0; i < cfg.overrideExtensions.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << "\"" << cfg.overrideExtensions[i] << "\"";
    }
    oss << "]\n"
        << "gpuDecodeMode = \"" << gpuModeStr(cfg.gpuDecodeMode) << "\"\n"
        << "reduceWarnings = " << (cfg.reduceWarnings ? "true" : "false") << "\n"
        << "debugLogs = " << (cfg.debugLogs ? "true" : "false") << "\n"
        << "disableMultithreading = " << (cfg.disableMultithreading ? "true" : "false") << "\n"
        << "disableThumbnailCache = " << (cfg.disableThumbnailCache ? "true" : "false") << "\n"
        << "disableFrameCache = " << (cfg.disableFrameCache ? "true" : "false") << "\n"
        << "disableSeekingTable = " << (cfg.disableSeekingTable ? "true" : "false") << "\n"
        << "maxFrameCacheSizeMB = " << cfg.maxFrameCacheSizeMB << "\n"
        << "maxPercentUsageAvailableMemory = " << cfg.maxPercentUsageAvailableMemory << "\n"
        << "maxLogFileSizeMB = " << cfg.maxLogFileSizeMB << "\n"
        << "thumbnailTimecode = \"" << thumbModeStr(cfg.thumbnailTimecode) << "\"\n"
        << "durationCalculation = \"" << durationModeStr(cfg.durationCalculation) << "\"\n";
    return oss.str();
}

bool ConfigTomlEditor::saveToFile(const std::string& path, const importer::OpenMediaConfig& cfg) {
    if (path.empty()) return false;
    const std::string tmpPath = path + ".tmp";
    {
        std::ofstream ofs(tmpPath, std::ios::trunc | std::ios::binary);
        if (!ofs.is_open()) return false;
        const std::string data = serializeToToml(cfg);
        ofs.write(data.data(), static_cast<std::streamsize>(data.size()));
        if (!ofs.good()) return false;
    }
    std::remove(path.c_str());
    return std::rename(tmpPath.c_str(), path.c_str()) == 0;
}

} // namespace openmedia::desktop
