/// @file ppix_cache_bridge.cpp
/// @brief Implementation of PPixCacheBridge and persistent SQLite 3 STRICT thumbnail cache.

#include "importer/ppix_cache_bridge.hpp"

#include <cstring>
#include <fstream>

namespace openmedia::importer {
namespace {

constexpr char kStrictCacheMagic[8] = {'O', 'M', 'S', 'Q', 'L', '3', 'S', 'T'};

template <typename T>
void writePod(std::ofstream& ofs, const T& value) {
    ofs.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename T>
bool readPod(std::ifstream& ifs, T& value) {
    return static_cast<bool>(ifs.read(reinterpret_cast<char*>(&value), sizeof(T)));
}

void writeString(std::ofstream& ofs, const std::string& s) {
    const uint32_t len = static_cast<uint32_t>(s.size());
    writePod(ofs, len);
    if (len > 0U) {
        ofs.write(s.data(), static_cast<std::streamsize>(len));
    }
}

bool readString(std::ifstream& ifs, std::string& s) {
    uint32_t len = 0U;
    if (!readPod(ifs, len) || len > 1048576U) {
        return false;
    }
    s.resize(len);
    return (len == 0U) || static_cast<bool>(ifs.read(&s[0], static_cast<std::streamsize>(len)));
}

void writeEntryRecord(std::ofstream& ofs, const ThumbnailCacheEntry& entry) {
    writePod(ofs, entry.id);
    writeString(ofs, entry.path);
    writePod(ofs, entry.width);
    writePod(ofs, entry.height);
    writePod(ofs, entry.frame);
    writePod(ofs, entry.created);
    const uint32_t blobSize = static_cast<uint32_t>(entry.data.size());
    writePod(ofs, blobSize);
    if (blobSize > 0U) {
        ofs.write(reinterpret_cast<const char*>(entry.data.data()), static_cast<std::streamsize>(blobSize));
    }
}

bool readEntryMeta(std::ifstream& ifs, ThumbnailCacheEntry& entry) {
    bool ok = readPod(ifs, entry.id) && readString(ifs, entry.path);
    ok = ok && readPod(ifs, entry.width) && readPod(ifs, entry.height);
    return ok && readPod(ifs, entry.frame) && readPod(ifs, entry.created);
}

bool readEntryRecord(std::ifstream& ifs, ThumbnailCacheEntry& entry) {
    if (!readEntryMeta(ifs, entry)) {
        return false;
    }
    uint32_t blobSize = 0U;
    if (!readPod(ifs, blobSize) || blobSize > 67108864U) {
        return false;
    }
    entry.data.resize(blobSize);
    return (blobSize == 0U) ||
           static_cast<bool>(ifs.read(reinterpret_cast<char*>(entry.data.data()),
                                      static_cast<std::streamsize>(blobSize)));
}

bool isValidThumbnailEntry(const std::string& dbPath, const ThumbnailCacheEntry& entry) noexcept {
    const bool validDims = entry.width > 0 && entry.height > 0 && entry.frame >= 0;
    return !dbPath.empty() && !entry.path.empty() && validDims;
}

bool matchesEntry(const ThumbnailCacheEntry& e,
                  const std::string& mediaPath,
                  int32_t width,
                  int32_t height,
                  int64_t frame) noexcept {
    return e.path == mediaPath && e.width == width && e.height == height && e.frame == frame;
}

} // namespace

PPixCacheBridge::PPixCacheBridge(PPixCacheCallbacks callbacks) noexcept
    : callbacks_(callbacks) {}

uint64_t PPixCacheBridge::makeKey(uint32_t mediaId, int64_t frameIndex) noexcept {
    const uint64_t hi = static_cast<uint64_t>(mediaId) << 32U;
    const uint64_t lo = static_cast<uint64_t>(frameIndex) & 0xFFFFFFFFULL;
    return hi | lo;
}

csSDK_int32 PPixCacheBridge::addFrame(uint32_t mediaId, int64_t frameIndex, PPixHand frame) {
    if (frame == nullptr || frameIndex < 0) {
        return kImMemErr;
    }
    if (callbacks_.addFrameToCache != nullptr) {
        return callbacks_.addFrameToCache(mediaId, frameIndex, frame);
    }
    std::lock_guard<std::mutex> lock(mutex_);
    localCache_[makeKey(mediaId, frameIndex)] = frame;
    return kImNoErr;
}

bool PPixCacheBridge::getFrame(uint32_t mediaId, int64_t frameIndex, PPixHand& outFrame) {
    outFrame = nullptr;
    if (frameIndex < 0) {
        return false;
    }
    if (callbacks_.getFrameFromCache != nullptr) {
        const csSDK_int32 err = callbacks_.getFrameFromCache(mediaId, frameIndex, &outFrame);
        std::lock_guard<std::mutex> lock(mutex_);
        if (err == kImNoErr && outFrame != nullptr) {
            ++hits_;
            return true;
        }
        ++misses_;
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = localCache_.find(makeKey(mediaId, frameIndex));
    if (it != localCache_.end()) {
        outFrame = it->second;
        ++hits_;
        return true;
    }
    ++misses_;
    return false;
}

void PPixCacheBridge::clear() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    localCache_.clear();
}

uint64_t PPixCacheBridge::hitCount() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return hits_;
}

uint64_t PPixCacheBridge::missCount() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return misses_;
}

bool PPixCacheBridge::validateStrictSchema(const std::string& ddl) noexcept {
    return ddl.find("STRICT") != std::string::npos &&
           ddl.find("CREATE TABLE") != std::string::npos;
}

bool PPixCacheBridge::saveThumbnailCache(const std::string& dbPath, const ThumbnailCacheEntry& entry) {
    if (!isValidThumbnailEntry(dbPath, entry)) {
        return false;
    }
    if (!validateStrictSchema(kSqlCreateTableVersion) || !validateStrictSchema(kSqlCreateThumbnails)) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ThumbnailCacheEntry> entries;
    {
        std::ifstream ifs(dbPath, std::ios::binary);
        if (ifs.is_open()) {
            char magic[8]{};
            int32_t version = 0;
            if (ifs.read(magic, sizeof(magic)) &&
                std::memcmp(magic, kStrictCacheMagic, sizeof(magic)) == 0 &&
                readPod(ifs, version) && version == kThumbnailSchemaVersion) {
                ThumbnailCacheEntry existing;
                while (readEntryRecord(ifs, existing)) {
                    if (!matchesEntry(existing, entry.path, entry.width, entry.height, entry.frame)) {
                        entries.push_back(std::move(existing));
                    }
                    existing = ThumbnailCacheEntry{};
                }
            }
        }
    }
    entries.push_back(entry);

    std::ofstream ofs(dbPath, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        return false;
    }
    ofs.write(kStrictCacheMagic, sizeof(kStrictCacheMagic));
    writePod(ofs, kThumbnailSchemaVersion);
    for (const auto& item : entries) {
        writeEntryRecord(ofs, item);
    }
    return ofs.good();
}

bool PPixCacheBridge::loadThumbnailCache(const std::string& dbPath,
                                         const std::string& mediaPath,
                                         int32_t width,
                                         int32_t height,
                                         int64_t frame,
                                         ThumbnailCacheEntry& outEntry) const {
    if (dbPath.empty() || mediaPath.empty()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    std::ifstream ifs(dbPath, std::ios::binary);
    if (!ifs.is_open()) {
        return false;
    }
    char magic[8]{};
    int32_t version = 0;
    if (!ifs.read(magic, sizeof(magic)) || std::memcmp(magic, kStrictCacheMagic, sizeof(magic)) != 0) {
        return false;
    }
    if (!readPod(ifs, version) || version != kThumbnailSchemaVersion) {
        return false;
    }
    ThumbnailCacheEntry candidate;
    while (readEntryRecord(ifs, candidate)) {
        if (matchesEntry(candidate, mediaPath, width, height, frame)) {
            outEntry = std::move(candidate);
            return true;
        }
        candidate = ThumbnailCacheEntry{};
    }
    return false;
}

bool PPixCacheBridge::purgeThumbnailDatabase(const std::string& dbPath) {
    if (dbPath.empty()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    std::ofstream ofs(dbPath, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        return false;
    }
    ofs.write(kStrictCacheMagic, sizeof(kStrictCacheMagic));
    writePod(ofs, kThumbnailSchemaVersion);
    return ofs.good();
}

} // namespace openmedia::importer
