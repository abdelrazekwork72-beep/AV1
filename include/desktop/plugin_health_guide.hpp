#pragma once
/// @file plugin_health_guide.hpp
/// @brief MediaCore 7.0 plugin verification and bilingual User Guide generator.

#include "desktop/desktop_types.hpp"

#include <string>
#include <vector>

namespace openmedia::desktop {

class PluginHealthAndGuide {
public:
    static std::string defaultMediaCoreDir();
    static std::vector<PluginHealthItem> inspectInstallation(const std::string& mediaCoreDir);
    static std::string generateBilingualUserGuide(UiLanguage lang);
};

} // namespace openmedia::desktop
