#include "desktop/plugin_health_guide.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

int main() {
    using namespace openmedia::desktop;
    namespace fs = std::filesystem;

    const fs::path tmpDir = fs::temp_directory_path() / "openmedia_health_test";
    fs::create_directories(tmpDir);
    {
        std::ofstream f1(tmpDir / "OpenMedia_Importer.prm");
        f1 << "prm";
        std::ofstream f2(tmpDir / "OpenMediaCore.dll");
        f2 << "dll";
    }

    auto items = PluginHealthAndGuide::inspectInstallation(tmpDir.string());
    assert(items.size() >= 4);
    bool foundImporter = false;
    for (const auto& item : items) {
        if (item.componentName == "OpenMedia_Importer.prm") {
            foundImporter = true;
            assert(item.present);
            assert(item.sizeBytes > 0);
        }
    }
    assert(foundImporter);

    const std::string enGuide = PluginHealthAndGuide::generateBilingualUserGuide(UiLanguage::English);
    const std::string arGuide = PluginHealthAndGuide::generateBilingualUserGuide(UiLanguage::Arabic);
    assert(!enGuide.empty() && !arGuide.empty() && enGuide != arGuide);

    fs::remove_all(tmpDir);
    std::cout << "test_plugin_health_guide: PASSED\n";
    return 0;
}
