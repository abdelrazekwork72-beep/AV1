#include "desktop/cache_log_manager.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

int main() {
    using namespace openmedia::desktop;
    namespace fs = std::filesystem;

    const fs::path tmpDir = fs::temp_directory_path() / "openmedia_cache_test";
    fs::create_directories(tmpDir);
    const fs::path thumbDb = tmpDir / "thumbs.db";
    {
        std::ofstream f(thumbDb);
        f << "cached_entry_data";
    }

    auto thumbRes = CacheAndLogManager::clearThumbnailCache(thumbDb.string());
    assert(thumbRes.success);

    const char* homeEnv = std::getenv("HOME");
    if (!homeEnv) homeEnv = std::getenv("USERPROFILE");
    if (homeEnv && *homeEnv) {
        const std::string redacted = CacheAndLogManager::redactUserPath(std::string(homeEnv) + "/Videos/clip.mkv");
        assert(redacted.find("<USER>") != std::string::npos);
    }

    const fs::path cfgPath = tmpDir / "Config.toml";
    const fs::path telPath = tmpDir / "telemetry.json";
    const fs::path diagPath = tmpDir / "diag.txt";
    {
        std::ofstream c(cfgPath); c << "[decode_policy]\nmax_gpu_streams = 4\n";
        std::ofstream t(telPath); t << "[]\n";
    }
    auto diagRes = CacheAndLogManager::exportDiagnosticBundle(cfgPath.string(), telPath.string(), diagPath.string());
    assert(diagRes.success);
    assert(fs::exists(diagPath));

    fs::remove_all(tmpDir);
    std::cout << "test_cache_log_manager: PASSED\n";
    return 0;
}
