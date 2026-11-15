#pragma once
/// @file config_toml_editor.hpp
/// @brief 16-Key Config.toml parser, validator, extension normalizer, and atomic writer.

#include "desktop/desktop_types.hpp"

#include <string>
#include <vector>

namespace openmedia::desktop {

class ConfigTomlEditor {
public:
    static std::vector<std::string> normalizeExtensions(const std::string& commaSeparated);
    static std::string formatExtensions(const std::vector<std::string>& exts);
    static importer::OpenMediaConfig clampDefaults(importer::OpenMediaConfig cfg);
    static importer::OpenMediaConfig loadFromFile(const std::string& path, bool* outRecoveredMalformed = nullptr);
    static bool saveToFile(const std::string& path, const importer::OpenMediaConfig& cfg);
    static std::string serializeToToml(const importer::OpenMediaConfig& cfg);
    static importer::OpenMediaConfig factoryDefaults();
};

} // namespace openmedia::desktop
