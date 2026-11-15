#include "importer/audio_extractor.hpp"
#include "importer/d3d11va_nvdec_decoder.hpp"
#include "importer/frame_scaler.hpp"
#include "importer/keyframe_ring_buffer.hpp"
#include "importer/multi_channel_router.hpp"
#include "importer/pixel_format_hdr.hpp"
#include "importer/ppix_cache_bridge.hpp"

#include <array>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <vector>

using namespace openmedia::importer;

namespace {

class MockDemuxer final : public openmedia::ICoreDemuxer {
public:
    bool closed = false;
    std::vector<openmedia::StreamInfo> streams;
    openmedia::CoreStatus openFile(const std::string&) override { return openmedia::CoreStatus::Ok; }
    openmedia::CoreStatus quietFile() override { return openmedia::CoreStatus::Ok; }
    openmedia::CoreStatus reopenIfQuieted() override { return openmedia::CoreStatus::Ok; }
    void close() override { closed = true; }
    const std::vector<openmedia::StreamInfo>& getStreams() const override { return streams; }
    openmedia::CoreStatus readNextPacket(openmedia::EncodedPacket&) override { return openmedia::CoreStatus::Ok; }
    openmedia::CoreStatus seekToTimestamp(int32_t, int64_t) override { return openmedia::CoreStatus::Ok; }
};

void testGpuDecodePolicyAnd4StreamCap() {
    D3D11VaNvdecDecoder::resetGlobalGpuStateForTesting();

    assert(D3D11VaNvdecDecoder::evaluateDecodePolicy(1920, 1080, GpuDecodeMode::Recommended) ==
           DecodePolicyDecision::UseCpu);
    assert(D3D11VaNvdecDecoder::evaluateDecodePolicy(2560, 1600, GpuDecodeMode::Recommended) ==
           DecodePolicyDecision::UseGpu);
    assert(D3D11VaNvdecDecoder::evaluateDecodePolicy(3840, 2160, GpuDecodeMode::Recommended) ==
           DecodePolicyDecision::UseGpu);
    assert(D3D11VaNvdecDecoder::evaluateDecodePolicy(1920, 1080, GpuDecodeMode::Available) ==
           DecodePolicyDecision::UseGpu);
    assert(D3D11VaNvdecDecoder::evaluateDecodePolicy(3840, 2160, GpuDecodeMode::Disabled) ==
           DecodePolicyDecision::UseCpu);

    std::array<D3D11VaNvdecDecoder, 5> decoders;
    for (size_t i = 0; i < 4; ++i) {
        assert(decoders[i].initializeWithPolicy(HwBackend::NVDEC, 3840, 2160, GpuDecodeMode::Recommended) ==
               HwBackend::NVDEC);
    }
    assert(D3D11VaNvdecDecoder::activeGpuStreamCount() == 4U);
    assert(decoders[4].initializeWithPolicy(HwBackend::NVDEC, 3840, 2160, GpuDecodeMode::Recommended) ==
           HwBackend::SoftwareFallback);

    assert(decoders[0].drainBeforeFallback(true));
    assert(decoders[0].activeBackend() == HwBackend::SoftwareFallback);
    assert(D3D11VaNvdecDecoder::activeGpuStreamCount() == 3U);
    assert(D3D11VaNvdecDecoder::hasLoggedFallbackWarning());

    D3D11VaNvdecDecoder::resetGlobalGpuStateForTesting();
}

void testSqliteStrictThumbnailCache() {
    assert(PPixCacheBridge::validateStrictSchema(PPixCacheBridge::kSqlCreateTableVersion));
    assert(PPixCacheBridge::validateStrictSchema(PPixCacheBridge::kSqlCreateThumbnails));

    PPixCacheBridge bridge;
    const std::string dbPath =
        (std::filesystem::temp_directory_path() / "openmedia_thumb_test.db").string();
    std::remove(dbPath.c_str());

    ThumbnailCacheEntry entry;
    entry.id = 1;
    entry.path = "/media/test_av1.mkv";
    entry.width = 320;
    entry.height = 180;
    entry.frame = 42;
    entry.created = 1794718588;
    entry.data = {0xAA, 0xBB, 0xCC, 0xDD};

    assert(bridge.saveThumbnailCache(dbPath, entry));

    ThumbnailCacheEntry loaded;
    assert(bridge.loadThumbnailCache(dbPath, "/media/test_av1.mkv", 320, 180, 42, loaded));
    assert(loaded.id == 1 && loaded.data == entry.data);
    assert(!bridge.loadThumbnailCache(dbPath, "/media/test_av1.mkv", 320, 180, 99, loaded));
    std::remove(dbPath.c_str());
}

void testExactLongGopOpenGopSeeking() {
    KeyframeRingBuffer ring(8);
    assert(ring.addKeyframe({0, 0, 100, false}));
    assert(ring.addKeyframe({1000, 30, 5000, true}));
    assert(ring.addKeyframe({2000, 60, 10000, false}));

    KeyframeEntry anchor;
    assert(ring.seekExactFrame(1000, anchor));
    assert(anchor.pts == 0);

    assert(ring.seekExactFrame(2000, anchor));
    assert(anchor.pts == 2000);
}

void testGpuColorConversionAndCameraSpaces() {
    const std::array<uint8_t, 4> y = {128, 128, 128, 128};
    const std::array<uint8_t, 2> uv = {128, 128};
    std::array<uint8_t, 16> bgra{};
    assert(PixelFormatHdrEngine::convertNv12ToBgra8Direct(
        y.data(), uv.data(), 2, 2, 2, 2, bgra.data(), 8, false));
    assert(bgra[3] == 255U);

    const std::array<uint16_t, 4> y10 = {512 << 6, 512 << 6, 512 << 6, 512 << 6};
    const std::array<uint16_t, 2> uv10 = {512 << 6, 512 << 6};
    assert(PixelFormatHdrEngine::convertP010ToBgra8OrRgb444Direct(
        y10.data(), uv10.data(), 2, 2, 2, 2, bgra.data(), 8, true));

    assert(PixelFormatHdrEngine::getColorSpaceMapping(ExtendedColorSpace::Sony_Cine_SLog3).isHdr);
    assert(PixelFormatHdrEngine::getColorSpaceMapping(ExtendedColorSpace::DCDM_XYZ).referenceGamma > 2.5f);
    assert(PixelFormatHdrEngine::applyTransferToLinear(0.5f, ExtendedColorSpace::BT2100_PQ) > 0.0f);
    assert(FrameScaler::mapExtendedColorSpace(ExtendedColorSpace::BT2100_HLG) == ColorMatrix::Rec2020);
}

void testFaultTolerantAudioAndMultiChannel() {
    std::array<float, 8> ch0{};
    std::array<float, 8> ch1{};
    float* bufs[2] = {ch0.data(), ch1.data()};
    ImImportAudioRec7Ext rec{};
    rec.buffer = bufs;
    rec.size = 8;

    assert(AudioExtractor::importAudio7FaultTolerant(&rec, {1000, -1000}, 2, true) == kImNoErr);
    for (float s : ch0) assert(s == 0.0f);
    for (float s : ch1) assert(s == 0.0f);

    MockDemuxer demuxer;
    assert(AudioExtractor::initializeAudioWithCleanup(&demuxer, 2, true) == kImBadFile);
    assert(demuxer.closed);

    ImAudioChannelLayoutRecExt layout5{};
    assert(MultiChannelRouter::resolveLayout(5, layout5) == kImNoErr);
    assert(layout5.channelLabels.size() == 5U);

    ImAudioChannelLayoutRecExt layout24{};
    assert(MultiChannelRouter::resolveLayout(24, layout24) == kImNoErr);
    assert(layout24.channelLabels.size() == 24U);

    ImAudioChannelLayoutRecExt layout32{};
    assert(MultiChannelRouter::resolveLayout(32, layout32) == kImNoErr);
    assert(layout32.channelLabels.size() == 32U);
}

} // namespace

int main() {
    testGpuDecodePolicyAnd4StreamCap();
    testSqliteStrictThumbnailCache();
    testExactLongGopOpenGopSeeking();
    testGpuColorConversionAndCameraSpaces();
    testFaultTolerantAudioAndMultiChannel();
    std::cout << "[PASS] test_influx_gpu_policy_and_sqlite: All US-2.2.0/2.3.0/2.4.0 tests passed.\n";
    return 0;
}
