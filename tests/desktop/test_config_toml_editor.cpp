#include "desktop/config_toml_editor.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

int main() {
    using namespace openmedia::desktop;
    namespace fs = std::filesystem;

    auto exts = ConfigTomlEditor::normalizeExtensions(" .MKV , webm, MP4 , .mkv ");
    assert(exts.size() == 3);
    assert(exts[0] == "mkv" && exts[1] == "webm" && exts[2] == "mp4");

    auto cfg = ConfigTomlEditor::factoryDefaults();
    cfg.uiLanguage = "ar";
    cfg.maxFrameCacheSizeMB = 1024;
    cfg.maxLogFileSizeMB = 32;
    cfg.importPolicy = "force_override_all_adobe";
    cfg.debugLogs = true;

    const fs::path tmpPath = fs::temp_directory_path() / "openmedia_test_config.toml";
    assert(ConfigTomlEditor::saveToFile(tmpPath.string(), cfg));

    bool recovered = false;
    auto loaded = ConfigTomlEditor::loadFromFile(tmpPath.string(), &recovered);
    assert(!recovered);
    assert(loaded.uiLanguage == "ar");
    assert(loaded.maxFrameCacheSizeMB == 1024);
    assert(loaded.maxLogFileSizeMB == 32);
    assert(loaded.importPolicy == "force_override_all_adobe");
    assert(loaded.debugLogs == true);

    {
        std::ofstream bad(tmpPath);
        // empty 0-byte file
    }
    bool badRecovered = false;
    auto fallback = ConfigTomlEditor::loadFromFile(tmpPath.string(), &badRecovered);
    assert(badRecovered);
    assert(fallback.maxFrameCacheSizeMB >= 64);

    fs::remove(tmpPath);
    std::cout << "test_config_toml_editor: PASSED\n";
    return 0;
}
