#pragma once
/// @file keyframe_ring_buffer.hpp
/// @brief Sparse O(log N) keyframe seek index, exact-frame Long-GOP/open-GOP seek, and ring buffer.

#include "openmedia/video_decoder.hpp"

#include <cstdint>
#include <vector>

namespace openmedia::importer {

/// @brief Sparse keyframe entry supporting CFR, VFR, and open-GOP timestamps.
struct KeyframeEntry {
    int64_t pts = 0;
    int64_t frameIndex = 0;
    int64_t byteOffset = 0;
    bool isOpenGop = false;
};

/// @brief Maintains O(log N) keyframe lookup and a fixed-capacity circular decoded-frame ring.
class KeyframeRingBuffer {
public:
    explicit KeyframeRingBuffer(size_t capacity = 32) noexcept;

    bool addKeyframe(const KeyframeEntry& entry);
    bool findPrecedingKeyframe(int64_t targetPts, KeyframeEntry& outEntry) const noexcept;

    /// @brief Finds the exact decode-start keyframe for Long-GOP / open-GOP streams (AVCHD/MTS, MXF, H.264/HEVC, AV1).
    bool seekExactFrame(int64_t targetPts, KeyframeEntry& outAnchor) const noexcept;

    void pushFrame(const RawVideoFrame& frame);
    bool findBufferedFrame(int64_t pts, RawVideoFrame& outFrame) const;

    size_t keyframeCount() const noexcept { return keyframes_.size(); }
    size_t bufferedCount() const noexcept { return count_; }
    void clear() noexcept;

private:
    std::vector<KeyframeEntry> keyframes_;
    std::vector<RawVideoFrame> ring_;
    size_t capacity_;
    size_t head_ = 0;
    size_t count_ = 0;
};

} // namespace openmedia::importer
