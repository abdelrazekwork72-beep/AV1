/// @file keyframe_ring_buffer.cpp
/// @brief Implementation of KeyframeRingBuffer with exact-frame Long-GOP / open-GOP seeking.

#include "importer/keyframe_ring_buffer.hpp"

#include <algorithm>

namespace openmedia::importer {

KeyframeRingBuffer::KeyframeRingBuffer(size_t capacity) noexcept
    : capacity_(capacity == 0 ? 1 : capacity) {
    ring_.resize(capacity_);
}

bool KeyframeRingBuffer::addKeyframe(const KeyframeEntry& entry) {
    if (!keyframes_.empty() && entry.pts < keyframes_.back().pts) {
        return false;
    }
    keyframes_.push_back(entry);
    return true;
}

bool KeyframeRingBuffer::findPrecedingKeyframe(int64_t targetPts, KeyframeEntry& outEntry) const noexcept {
    if (keyframes_.empty()) {
        return false;
    }
    auto it = std::upper_bound(
        keyframes_.begin(), keyframes_.end(), targetPts,
        [](int64_t val, const KeyframeEntry& e) { return val < e.pts; });
    if (it == keyframes_.begin()) {
        outEntry = keyframes_.front();
        return true;
    }
    --it;
    outEntry = *it;
    return true;
}

bool KeyframeRingBuffer::seekExactFrame(int64_t targetPts, KeyframeEntry& outAnchor) const noexcept {
    if (keyframes_.empty()) {
        return false;
    }
    auto it = std::upper_bound(
        keyframes_.begin(), keyframes_.end(), targetPts,
        [](int64_t val, const KeyframeEntry& e) { return val < e.pts; });
    if (it != keyframes_.begin()) {
        --it;
    }
    if (it->isOpenGop && it != keyframes_.begin() && targetPts == it->pts) {
        --it;
    }
    outAnchor = *it;
    return true;
}

void KeyframeRingBuffer::pushFrame(const RawVideoFrame& frame) {
    ring_[head_] = frame;
    head_ = (head_ + 1) % capacity_;
    if (count_ < capacity_) {
        ++count_;
    }
}

bool KeyframeRingBuffer::findBufferedFrame(int64_t pts, RawVideoFrame& outFrame) const {
    for (size_t i = 0; i < count_; ++i) {
        if (ring_[i].pts == pts) {
            outFrame = ring_[i];
            return true;
        }
    }
    return false;
}

void KeyframeRingBuffer::clear() noexcept {
    keyframes_.clear();
    head_ = 0;
    count_ = 0;
}

} // namespace openmedia::importer
