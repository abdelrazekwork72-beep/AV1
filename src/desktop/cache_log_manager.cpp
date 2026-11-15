#include "desktop/cache_log_manager.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace openmedia::desktop {
namespace {

bool isAllowedMediaCacheExt(const std::string& ext) {
    return ext == ".pek" || ext == ".cfa" || ext == ".ims" || ext == ".mcdb";
}

} // namespace

std::string CacheAndLogManager::defaultThumbnailCachePath() {
#ifdef _WIN32
    const char* appData = std::getenv("APPDATA");
    return std::string(appData ? appData : ".") + "\\OpenMedia\\thumbnails_cache.db";
#else
    const char* home = std::getenv("HOME");
    return std::string(home ? home : ".") + "/.config/OpenMedia/thumbnails_cache.db";
#endif
}

std::string CacheAndLogManager::defaultPremiereMediaCacheDir() {
#ifdef _WIN32
    const char* appData = std::getenv("APPDATA");
    return std::string(appData ? appData : ".") + "\\Adobe\\Common\\Media Cache Files";
#else
    const char* home = std::getenv("HOME");
    return std::string(home ? home : ".") + "/Library/Application Support/Adobe/Common/Media Cache Files";
#endif
}

CachePurgeSummary CacheAndLogManager::clearThumbnailCache(const std::string& dbPath) {
    CachePurgeSummary summary;
    if (dbPath.empty()) {
        summary.statusMessage = "Empty database path";
        return summary;
    }
    std::error_code ec;
    uint64_t beforeSize = 0;
    if (std::filesystem::exists(dbPath, ec)) {
        beforeSize = std::filesystem::file_size(dbPath, ec);
    }
    importer::PPixCacheBridge bridge;
    if (!bridge.purgeThumbnailDatabase(dbPath)) {
        summary.statusMessage = "Failed to purge thumbnail database";
        return summary;
    }
    const uint64_t afterSize = std::filesystem::exists(dbPath, ec) ? std::filesystem::file_size(dbPath, ec) : 0ULL;
    summary.success = true;
    summary.filesRemoved = 1;
    summary.bytesRecovered = (beforeSize > afterSize) ? (beforeSize - afterSize) : 0ULL;
    summary.statusMessage = "Thumbnail cache purged";
    return summary;
}

CachePurgeSummary CacheAndLogManager::clearPremiereMediaCache(const std::string& mediaCacheDir) {
    CachePurgeSummary summary;
    std::error_code ec;
    if (mediaCacheDir.empty() || !std::filesystem::exists(mediaCacheDir, ec) ||
        !std::filesystem::is_directory(mediaCacheDir, ec)) {
        summary.statusMessage = "Media Cache directory not found";
        return summary;
    }
    for (const auto& entry : std::filesystem::directory_iterator(mediaCacheDir, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        const std::string ext = entry.path().extension().string();
        if (!isAllowedMediaCacheExt(ext)) continue;
        const uint64_t sz = entry.file_size(ec);
        if (std::filesystem::remove(entry.path(), ec)) {
            ++summary.filesRemoved;
            summary.bytesRecovered += sz;
        }
    }
    summary.success = true;
    summary.statusMessage = "Media cache cleaned";
    return summary;
}

std::string CacheAndLogManager::redactUserPath(const std::string& input) {
    std::string out = input;
    for (const char* envKey : {"USERPROFILE", "HOME"}) {
        const char* val = std::getenv(envKey);
        if (val != nullptr && *val != '\0') {
            const std::string needle(val);
            size_t pos = 0;
            while ((pos = out.find(needle, pos)) != std::string::npos) {
                out.replace(pos, needle.size(), "<USER>");
                pos += 6U;
            }
        }
    }
    return out;
}

CachePurgeSummary CacheAndLogManager::exportDiagnosticBundle(const std::string& configPath,
                                                              const std::string& telemetryPath,
                                                              const std::string& outBundlePath) {
    CachePurgeSummary summary;
    std::ofstream ofs(outBundlePath, std::ios::trunc);
    if (!ofs.is_open()) {
        summary.statusMessage = "Cannot create diagnostic bundle";
        return summary;
    }
    ofs << "=== OpenMedia Diagnostic Bundle ===\n";
    for (const auto& pair : {std::pair<const char*, std::string>{"Config.toml", configPath},
                             std::pair<const char*, std::string>{"Telemetry", telemetryPath}}) {
        ofs << "--- " << pair.first << " ---\n";
        std::ifstream ifs(pair.second);
        if (ifs.is_open()) {
            std::ostringstream ss;
            ss << ifs.rdbuf();
            ofs << redactUserPath(ss.str()) << "\n";
        } else {
            ofs << "<not found>\n";
        }
    }
    summary.success = ofs.good();
    summary.filesRemoved = 1;
    summary.statusMessage = summary.success ? "Diagnostic bundle exported" : "Write error";
    return summary;
}

} // namespace openmedia::desktop
