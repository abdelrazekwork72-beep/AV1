#pragma once
/// @file async_importer.hpp
/// @brief Reentrant asynchronous importer engine (imCreateAsyncImporter / AsyncImporterEntry).

#include "importer/importer_types.hpp"
#include "openmedia/video_decoder.hpp"

#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace openmedia::importer {

/// @brief Reentrant asynchronous importer session managing lookahead frame prefetching.
class AsyncImporterEngine {
public:
    /// @brief Construct an async importer engine with optional lookahead window size.
    /// @param lookaheadFrames Maximum number of prefetched frames to retain.
    explicit AsyncImporterEngine(size_t lookaheadFrames = 8) noexcept;

    /// @brief Handle Adobe imCreateAsyncImporter selector.
    /// @param rec Extended async importer creation record.
    /// @return kImNoErr on success, kImMemErr on invalid arguments.
    static csSDK_int32 handleCreateAsyncImporter(ImAsyncImporterCreationRecExt* rec) noexcept;

    /// @brief Reentrant async selector entry point (AsyncImporterEntryFn).
    /// @param selector Async selector code.
    /// @param stdParms Standard importer parameters.
    /// @param param1 Opaque async private data or frame pointer.
    /// @param param2 Optional frame index pointer.
    /// @return kImNoErr on success, kImBadFile or kImMemErr on invalid state.
    static csSDK_int32 asyncImporterEntry(csSDK_int32 selector,
                                          imStdParms* stdParms,
                                          void* param1,
                                          void* param2) noexcept;

    /// @brief Teardown callback (AsyncImporterCloseFn).
    /// @param asyncPrivateData Engine instance pointer.
    /// @return kImNoErr.
    static csSDK_int32 asyncClose(void* asyncPrivateData) noexcept;

    /// @brief Prefetch and store a frame in the thread-safe lookahead map.
    bool storePrefetchedFrame(int64_t frameIndex, RawVideoFrame frame);

    /// @brief Retrieve a prefetched frame if present.
    bool tryGetPrefetchedFrame(int64_t frameIndex, RawVideoFrame& outFrame) const;

    /// @brief Close and clear all prefetched frames.
    void close() noexcept;

    /// @brief Query whether the engine has been closed.
    bool isClosed() const noexcept;

    /// @brief Query current number of cached lookahead frames.
    size_t cachedCount() const noexcept;

private:
    size_t lookaheadFrames_;
    bool closed_ = false;
    mutable std::mutex mutex_;
    std::unordered_map<int64_t, RawVideoFrame> cache_;
};

} // namespace openmedia::importer
