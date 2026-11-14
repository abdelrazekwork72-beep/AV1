#pragma once
/// @file ppix_cache_bridge.hpp
/// @brief Bridge to Adobe PrSDKPPixCacheSuite for frame and thumbnail caching.

#include "importer/importer_types.hpp"

#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace openmedia::importer {

/// @brief Callback table mirroring Adobe PrSDKPPixCacheSuite.
struct PPixCacheCallbacks {
    /// @brief Add a rendered PPixHand to Premiere's cache.
    csSDK_int32 (*addFrameToCache)(uint32_t mediaId, int64_t frameIndex, PPixHand frame) = nullptr;
    /// @brief Query a cached PPixHand from Premiere's cache.
    csSDK_int32 (*getFrameFromCache)(uint32_t mediaId, int64_t frameIndex, PPixHand* outFrame) = nullptr;
};

/// @brief Caches decoded timeline frames and Media Browser thumbnails via PrSDKPPixCacheSuite.
class PPixCacheBridge {
public:
    /// @brief Construct bridge with optional external Adobe cache suite callbacks.
    /// @param callbacks Suite function pointers (falls back to internal map if null).
    explicit PPixCacheBridge(PPixCacheCallbacks callbacks = {}) noexcept;

    /// @brief Store a decoded PPixHand in cache.
    /// @param mediaId Unique media identifier.
    /// @param frameIndex Frame number.
    /// @param frame Valid PPixHand handle.
    /// @return kImNoErr on success, kImMemErr on invalid input.
    csSDK_int32 addFrame(uint32_t mediaId, int64_t frameIndex, PPixHand frame);

    /// @brief Look up a previously cached PPixHand.
    /// @param mediaId Unique media identifier.
    /// @param frameIndex Frame number.
    /// @param outFrame Receives cached PPixHand on hit.
    /// @return true on cache hit, false on miss.
    bool getFrame(uint32_t mediaId, int64_t frameIndex, PPixHand& outFrame);

    /// @brief Clear internal fallback cache entries.
    void clear() noexcept;

    /// @brief Total cache hits recorded.
    uint64_t hitCount() const noexcept;

    /// @brief Total cache misses recorded.
    uint64_t missCount() const noexcept;

private:
    static uint64_t makeKey(uint32_t mediaId, int64_t frameIndex) noexcept;

    PPixCacheCallbacks callbacks_;
    mutable std::mutex mutex_;
    std::unordered_map<uint64_t, PPixHand> localCache_;
    uint64_t hits_ = 0;
    uint64_t misses_ = 0;
};

} // namespace openmedia::importer
