#include "importer/ppix_cache_bridge.hpp"

namespace openmedia::importer {

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

} // namespace openmedia::importer
