/// @file d3d11va_nvdec_decoder.cpp
/// @brief Implementation of D3D11VaNvdecDecoder with smart policy, 4-stream GPU cap, and drain fallback.

#include "importer/d3d11va_nvdec_decoder.hpp"

namespace openmedia::importer {
namespace {

std::atomic<uint32_t> gActiveGpuStreams{0U};
std::atomic<bool> gFallbackWarningLogged{false};

bool isHardwareBackend(HwBackend backend) noexcept {
    return backend == HwBackend::D3D11VA || backend == HwBackend::NVDEC;
}

} // namespace

D3D11VaNvdecDecoder::~D3D11VaNvdecDecoder() {
    releaseSlot();
}


DecodePolicyDecision D3D11VaNvdecDecoder::evaluateDecodePolicy(uint32_t width,
                                                               uint32_t height,
                                                               GpuDecodeMode mode) noexcept {
    if (mode == GpuDecodeMode::Disabled || width == 0U || height == 0U) {
        return DecodePolicyDecision::UseCpu;
    }
    if (gActiveGpuStreams.load(std::memory_order_acquire) >= kMaxConcurrentGpuDecoders) {
        return DecodePolicyDecision::UseCpu;
    }
    const uint64_t pixels = static_cast<uint64_t>(width) * static_cast<uint64_t>(height);
    if (mode == GpuDecodeMode::Recommended && pixels < kRecommendedMinGpuPixels) {
        return DecodePolicyDecision::UseCpu;
    }
    return DecodePolicyDecision::UseGpu;
}

uint32_t D3D11VaNvdecDecoder::activeGpuStreamCount() noexcept {
    return gActiveGpuStreams.load(std::memory_order_acquire);
}

void D3D11VaNvdecDecoder::resetGlobalGpuStateForTesting() noexcept {
    gActiveGpuStreams.store(0U, std::memory_order_release);
    gFallbackWarningLogged.store(false, std::memory_order_release);
}

bool D3D11VaNvdecDecoder::hasLoggedFallbackWarning() noexcept {
    return gFallbackWarningLogged.load(std::memory_order_acquire);
}

bool D3D11VaNvdecDecoder::tryAcquireGpuSlot() noexcept {
    if (holdsGpuSlot_) {
        return true;
    }
    uint32_t cur = gActiveGpuStreams.load(std::memory_order_acquire);
    while (cur < kMaxConcurrentGpuDecoders) {
        if (gActiveGpuStreams.compare_exchange_weak(cur, cur + 1U, std::memory_order_acq_rel)) {
            holdsGpuSlot_ = true;
            return true;
        }
    }
    return false;
}

void D3D11VaNvdecDecoder::releaseSlot() noexcept {
    if (!holdsGpuSlot_) {
        return;
    }
    holdsGpuSlot_ = false;
    uint32_t cur = gActiveGpuStreams.load(std::memory_order_acquire);
    while (cur > 0U) {
        if (gActiveGpuStreams.compare_exchange_weak(cur, cur - 1U, std::memory_order_acq_rel)) {
            return;
        }
    }
}

HwBackend D3D11VaNvdecDecoder::initialize(HwBackend preferred, bool simulateHwFailure) noexcept {
    releaseSlot();
    if (simulateHwFailure || !isHardwareBackend(preferred)) {
        active_ = HwBackend::SoftwareFallback;
        return active_;
    }
    if (!tryAcquireGpuSlot()) {
        active_ = HwBackend::SoftwareFallback;
        return active_;
    }
    active_ = preferred;
    return active_;
}

HwBackend D3D11VaNvdecDecoder::initializeWithPolicy(HwBackend preferred,
                                                    uint32_t width,
                                                    uint32_t height,
                                                    GpuDecodeMode mode,
                                                    bool simulateHwFailure) noexcept {
    if (evaluateDecodePolicy(width, height, mode) == DecodePolicyDecision::UseCpu) {
        releaseSlot();
        active_ = HwBackend::SoftwareFallback;
        return active_;
    }
    return initialize(preferred, simulateHwFailure);
}

bool D3D11VaNvdecDecoder::drainBeforeFallback(bool simulateError) noexcept {
    releaseSlot();
    active_ = HwBackend::SoftwareFallback;
    if (simulateError) {
        gFallbackWarningLogged.exchange(true, std::memory_order_acq_rel);
    }
    return true;
}

bool D3D11VaNvdecDecoder::transferAlignedSurface(uint32_t width,
                                                 uint32_t height,
                                                 bool is10Bit,
                                                 std::vector<uint8_t>& outBuffer,
                                                 uint32_t& outStride) const {
    if (width == 0 || height == 0 || width > 16384U || height > 16384U) {
        return false;
    }
    const uint32_t bytesPerSample = is10Bit ? 2U : 1U;
    const uint32_t rawBytes = width * bytesPerSample;
    outStride = ((rawBytes + 15U) / 16U) * 16U;
    const uint32_t totalHeight = height + (height + 1U) / 2U;
    outBuffer.assign(static_cast<size_t>(outStride) * totalHeight, 0U);
    return true;
}

} // namespace openmedia::importer
