#pragma once
/// @file keyframe_ring_buffer.hpp
/// @brief Sparse O(log N) keyframe seek index and circular decoded-frame ring buffer.

#include "openmedia/video_decoder.hpp"

#include <cstdint>
#include <vector>

namespace openmedia::importer {

/// @brief Sparse keyframe entry supporting CFR and VFR timestamps.
struct KeyframeEntry {
    int64_t pts = 0;        ///< Presentation timestamp in stream timebase units.
    int64_t frameIndex = 0; ///< Zero-based frame index.
    int64_t byteOffset = 0; ///< Byte offset in container for fast seek.
};

/// @brief Maintains O(log N) keyframe lookup and a fixed-capacity circular decoded-frame ring.
class KeyframeRingBuffer {
public:
    /// @brief Construct ring buffer with specified frame slot capacity.
    /// @param capacity Maximum number of decoded frames held in the ring.
    explicit KeyframeRingBuffer(size_t capacity = 32) noexcept;

    /// @brief Append a keyframe entry in monotonically non-decreasing PTS order.
    /// @param entry Keyframe metadata.
    /// @return true if inserted, false if out-of-order.
    bool addKeyframe(const KeyframeEntry& entry);

    /// @brief Find the nearest preceding keyframe at or before targetPts in O(log N).
    /// @param targetPts Target timestamp.
    /// @param outEntry Receives matching keyframe.
    /// @return true if found, false if index is empty.
    bool findPrecedingKeyframe(int64_t targetPts, KeyframeEntry& outEntry) const noexcept;

    /// @brief Push a decoded frame into the circular ring buffer, overwriting oldest if full.
    /// @param frame Decoded video frame.
    void pushFrame(const RawVideoFrame& frame);

    /// @brief Look up a decoded frame by PTS in the circular ring.
    /// @param pts Target presentation timestamp.
    /// @param outFrame Output frame copy.
    /// @return true on ring hit, false on miss.
    bool findBufferedFrame(int64_t pts, RawVideoFrame& outFrame) const;

    /// @brief Return number of indexed keyframes.
    size_t keyframeCount() const noexcept { return keyframes_.size(); }

    /// @brief Return current number of frames stored in the ring.
    size_t bufferedCount() const noexcept { return count_; }

    /// @brief Clear both keyframe index and ring slots.
    void clear() noexcept;

private:
    std::vector<KeyframeEntry> keyframes_;
    std::vector<RawVideoFrame> ring_;
    size_t capacity_;
    size_t head_ = 0;
    size_t count_ = 0;
};

} // namespace openmedia::importer
