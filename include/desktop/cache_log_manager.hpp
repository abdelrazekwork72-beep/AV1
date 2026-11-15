#pragma once
/// @file cache_log_manager.hpp
/// @brief Thumbnail cache VACUUM, Premiere Media Cache cleaner & diagnostic bundle exporter.

#include "desktop/desktop_types.hpp"
#include "importer/ppix_cache_bridge.hpp"

#include <string>

namespace openmedia::desktop {

class CacheAndLogManager {
public:
    static std::string defaultThumbnailCachePath();
    static std::string defaultPremiereMediaCacheDir();

    static CachePurgeSummary clearThumbnailCache(const std::string& dbPath);
    static CachePurgeSummary clearPremiereMediaCache(const std::string& mediaCacheDir);
    static std::string redactUserPath(const std::string& input);
    static CachePurgeSummary exportDiagnosticBundle(const std::string& configPath,
                                                    const std::string& telemetryPath,
                                                    const std::string& outBundlePath);
};

} // namespace openmedia::desktop
