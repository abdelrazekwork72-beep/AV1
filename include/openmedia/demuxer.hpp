#pragma once
/// @file demuxer.hpp
/// @brief Concrete RAII container demuxer declaration for OpenMediaCore.
/// @note Zero FFmpeg headers and zero proprietary Adobe SDK headers may be included here.

#include "openmedia/core_types.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace openmedia {

/// @brief Thread-safe, Pimpl-isolated container demuxer implementing ICoreDemuxer.
/// @details Wraps libavformat demuxing, stream discovery, rational timebase sanitization,
///          keyframe-accurate seeking, and OS file-descriptor quieting/reopening.
class CoreDemuxer final : public ICoreDemuxer {
public:
    /// @brief Constructs an uninitialized CoreDemuxer instance.
    CoreDemuxer();

    /// @brief Destroys the CoreDemuxer and releases all underlying media resources.
    ~CoreDemuxer() override;

    /// @brief Deleted copy constructor (demuxer owns unique OS/media handles).
    CoreDemuxer(const CoreDemuxer&) = delete;

    /// @brief Deleted copy assignment operator.
    CoreDemuxer& operator=(const CoreDemuxer&) = delete;

    /// @brief Move constructor transferring demuxer ownership.
    CoreDemuxer(CoreDemuxer&& other) noexcept;

    /// @brief Move assignment operator transferring demuxer ownership.
    CoreDemuxer& operator=(CoreDemuxer&& other) noexcept;

    /// @brief Opens a media container file and discovers all contained streams.
    /// @param path UTF-8 filesystem path to the input container file.
    /// @return CoreStatus::Ok on success, InvalidArgument if path is empty, or OpenFailed on error.
    CoreStatus openFile(const std::string& path) override;

    /// @brief Releases OS file handles while preserving stream metadata for fast reopen.
    /// @return CoreStatus::Ok on success or NotInitialized if no file is open.
    CoreStatus quietFile() override;

    /// @brief Reopens the media file if it was previously quieted via quietFile().
    /// @return CoreStatus::Ok if active or reopened, NotInitialized if closed, or OpenFailed on error.
    CoreStatus reopenIfQuieted() override;

    /// @brief Closes the demuxer, clears discovered streams, and releases all resources.
    void close() override;

    /// @brief Returns discovered stream metadata for the currently opened container.
    /// @return Const reference to the internal vector of StreamInfo descriptors.
    const std::vector<StreamInfo>& getStreams() const override;

    /// @brief Reads the next compressed packet from the container.
    /// @param outPacket Output packet populated with stream index, timestamps, flags, and payload.
    /// @return CoreStatus::Ok, EndOfFile, NotInitialized, or DecodeFailed.
    CoreStatus readNextPacket(EncodedPacket& outPacket) override;

    /// @brief Seeks to the nearest keyframe at or before timestampPts in the given stream.
    /// @param streamIndex Zero-based target stream index.
    /// @param timestampPts Target presentation timestamp in the stream's timeBase units.
    /// @return CoreStatus::Ok, NotInitialized, InvalidArgument, or SeekFailed.
    CoreStatus seekToTimestamp(int32_t streamIndex, int64_t timestampPts) override;

    /// @brief Checks whether a container is currently open (including while quieted).
    /// @return True if openFile() succeeded and close() has not been called.
    bool isOpen() const noexcept;

    /// @brief Checks whether the demuxer has temporarily released its OS file handle.
    /// @return True if quietFile() is active and reopenIfQuieted() has not yet been called.
    bool isQuieted() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace openmedia
