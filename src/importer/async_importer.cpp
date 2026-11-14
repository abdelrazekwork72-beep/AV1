#include "importer/async_importer.hpp"

#include <new>
#include <utility>

namespace openmedia::importer {

AsyncImporterEngine::AsyncImporterEngine(size_t lookaheadFrames) noexcept
    : lookaheadFrames_(lookaheadFrames == 0 ? 1 : lookaheadFrames) {}

csSDK_int32 AsyncImporterEngine::handleCreateAsyncImporter(ImAsyncImporterCreationRecExt* rec) noexcept {
    if (rec == nullptr) {
        return kImMemErr;
    }
    auto* engine = new (std::nothrow) AsyncImporterEngine(8);
    if (engine == nullptr) {
        return kImMemErr;
    }
    rec->outAsyncEntry = &AsyncImporterEngine::asyncImporterEntry;
    rec->outAsyncClose = &AsyncImporterEngine::asyncClose;
    rec->inPrivateData = engine;
    return kImNoErr;
}

csSDK_int32 AsyncImporterEngine::asyncImporterEntry(csSDK_int32 selector,
                                                      imStdParms* /*stdParms*/,
                                                      void* param1,
                                                      void* param2) noexcept {
    auto* engine = static_cast<AsyncImporterEngine*>(param1);
    if (engine == nullptr) {
        return kImMemErr;
    }
    if (selector == kSelCloseFile) {
        engine->close();
        return kImNoErr;
    }
    if (engine->isClosed()) {
        return kImBadFile;
    }
    const int64_t frameIndex = (param2 != nullptr) ? *static_cast<const int64_t*>(param2) : 0;
    if (frameIndex < 0) {
        return kImBadFile;
    }
    RawVideoFrame cached;
    engine->tryGetPrefetchedFrame(frameIndex, cached);
    return kImNoErr;
}

csSDK_int32 AsyncImporterEngine::asyncClose(void* asyncPrivateData) noexcept {
    auto* engine = static_cast<AsyncImporterEngine*>(asyncPrivateData);
    if (engine == nullptr) {
        return kImNoErr;
    }
    engine->close();
    delete engine;
    return kImNoErr;
}

bool AsyncImporterEngine::storePrefetchedFrame(int64_t frameIndex, RawVideoFrame frame) {
    if (frameIndex < 0 || frame.width <= 0 || frame.height <= 0) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_) {
        return false;
    }
    if (cache_.size() >= lookaheadFrames_ && cache_.find(frameIndex) == cache_.end()) {
        cache_.erase(cache_.begin());
    }
    cache_[frameIndex] = std::move(frame);
    return true;
}

bool AsyncImporterEngine::tryGetPrefetchedFrame(int64_t frameIndex, RawVideoFrame& outFrame) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_.find(frameIndex);
    if (it == cache_.end()) {
        return false;
    }
    outFrame = it->second;
    return true;
}

void AsyncImporterEngine::close() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    closed_ = true;
    cache_.clear();
}

bool AsyncImporterEngine::isClosed() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return closed_;
}

size_t AsyncImporterEngine::cachedCount() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.size();
}

} // namespace openmedia::importer
