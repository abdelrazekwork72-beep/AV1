#pragma once
/// @file ppix_cache_bridge.hpp
/// @brief Bridge to Adobe PrSDKPPixCacheSuite and persistent SQLite 3 STRICT Thumbnail Cache.

#include "importer/importer_types.hpp"

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace openmedia::importer {

/// @brief Callback table mirroring Adobe PrSDKPPixCacheSuite.
struct PPixCacheCallbacks {
    csSDK_int32 (*addFrameToCache)(uint32_t mediaId, int64_t frameIndex, PPixHand frame) = nullptr;
    csSDK_int32 (*getFrameFromCache)(uint32_t mediaId, int64_t frameIndex, PPixHand* outFrame) = nullptr;
};

/// @brief Persistent thumbnail record matching the SQLite 3 STRICT schema.
struct ThumbnailCacheEntry {
    int64_t id = 0;
    std::string path;
    int32_t width = 0;
    int32_t height = 0;
    int64_t frame = 0;
    int64_t created = 0;
    std::vector<uint8_t> data;
};

/// @brief Caches decoded timeline frames and persistent SQLite STRICT thumbnails.
class PPixCacheBridge {
public:
    static constexpr const char* kSqlCreateTableVersion =
        "CREATE TABLE IF NOT EXISTS tableVersion(name TEXT PRIMARY KEY, version INTEGER NOT NULL) STRICT;";
    static constexpr const char* kSqlCreateThumbnails =
        "CREATE TABLE IF NOT EXISTS thumbnails("
        "id INTEGER PRIMARY KEY, "
        "path TEXT NOT NULL, "
        "width INTEGER NOT NULL, "
        "height INTEGER NOT NULL, "
        "frame INTEGER NOT NULL, "
        "created INTEGER NOT NULL, "
        "data BLOB NOT NULL) STRICT;";
    static constexpr int32_t kThumbnailSchemaVersion = 1;

    explicit PPixCacheBridge(PPixCacheCallbacks callbacks = {}) noexcept;

    csSDK_int32 addFrame(uint32_t mediaId, int64_t frameIndex, PPixHand frame);
    bool getFrame(uint32_t mediaId, int64_t frameIndex, PPixHand& outFrame);
    void clear() noexcept;
    uint64_t hitCount() const noexcept;
    uint64_t missCount() const noexcept;

    /// @brief Validates that a SQLite DDL statement enforces STRICT typing.
    static bool validateStrictSchema(const std::string& ddl) noexcept;

    /// @brief Persists a thumbnail entry to the SQLite STRICT cache file.
    bool saveThumbnailCache(const std::string& dbPath, const ThumbnailCacheEntry& entry);

    /// @brief Loads a matching thumbnail entry from the SQLite STRICT cache file.
    bool loadThumbnailCache(const std::string& dbPath,
                            const std::string& mediaPath,
                            int32_t width,
                            int32_t height,
                            int64_t frame,
                            ThumbnailCacheEntry& outEntry) const;

    /// @brief Purges all cached thumbnails from the SQLite STRICT cache file (VACUUM).
    bool purgeThumbnailDatabase(const std::string& dbPath);

private:
    static uint64_t makeKey(uint32_t mediaId, int64_t frameIndex) noexcept;

    PPixCacheCallbacks callbacks_;
    mutable std::mutex mutex_;
    std::unordered_map<uint64_t, PPixHand> localCache_;
    uint64_t hits_ = 0;
    uint64_t misses_ = 0;
};

} // namespace openmedia::importer
