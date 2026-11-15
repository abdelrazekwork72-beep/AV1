#include "importer/importer_entry.hpp"
#include "importer/file_probe.hpp"

#include <cassert>
#include <iostream>

using namespace openmedia::importer;

namespace {

void testDualPrmEntryDispatcher() {
    imStdParms stdParms{};
    ImImportInfoRecExt stdInfo{};
    ImImportInfoRecExt hpInfo{};

    assert(xOpenMediaImportEntry(kSelInit, &stdParms, &stdInfo, nullptr, false) == kImNoErr);
    assert(stdInfo.priority == kStandardImporterPriority);
    assert(stdInfo.priority == 100);

    assert(xOpenMediaImportEntry(kSelInit, &stdParms, &hpInfo, nullptr, true) == kImNoErr);
    assert(hpInfo.priority == kHighImporterPriority);
    assert(hpInfo.priority == 200);
}

void testOpenMediaConfigToml() {
    const std::string toml = R"(
# OpenMedia Config
disable = false
overrideAdobe = true
overrideExtensions = [".mkv", "MP4", "WebM"]
disableAutomaticOverride = true
reduceWarnings = true
gpuDecodeMode = "available"
maxFrameCacheSizeMB = 512
maxPercentUsageAvailableMemory = 60
disableSeekingTable = true
disableThumbnailCache = false
thumbnailTimecode = "hide_when_zero"
durationCalculation = "frame_timestamp"
)";
    const OpenMediaConfig cfg = OpenMediaConfig::parseConfigToml(toml);
    assert(!cfg.disable);
    assert(cfg.overrideAdobe);
    assert(cfg.isExtensionOverridden("MKV"));
    assert(cfg.isExtensionOverridden(".mp4"));
    assert(cfg.isExtensionOverridden("webm"));
    assert(!cfg.isExtensionOverridden("avi"));
    assert(cfg.disableAutomaticOverride);
    assert(cfg.reduceWarnings);
    assert(cfg.gpuDecodeMode == GpuDecodeMode::Available);
    assert(cfg.maxFrameCacheSizeMB == 512U);
    assert(cfg.maxPercentUsageAvailableMemory == 60U);
    assert(cfg.disableSeekingTable);
    assert(!cfg.disableThumbnailCache);
    assert(cfg.thumbnailTimecode == ThumbnailTimecodeMode::HideWhenZero);
    assert(cfg.durationCalculation == DurationCalculationMode::FrameTimestamp);
}

void testFilenameModifiers() {
    const FilenameModifiers m1 = FilenameModifiers::parseFilenameModifiers(
        "/media/clip!w=1920!vs=1!as=2!pf=yuv420p10u!fr=24000d1001!cr=full!accurate_duration!rft=1.mkv.influx");
    assert(m1.cleanPath == "/media/clip.mkv");
    assert(m1.width == 1920);
    assert(m1.height == 0);
    assert(m1.videoStreamIndex == 1);
    assert(m1.audioStreamIndex == 2);
    assert(m1.pixelFormat == "yuv420p10u");
    assert(m1.frameRate.num == 24000 && m1.frameRate.den == 1001);
    assert(m1.colorRange == openmedia::ColorRange::Full);
    assert(m1.accurateDuration);
    assert(m1.resetFirstTimestamp);

    int32_t w = m1.width;
    int32_t h = m1.height;
    FilenameModifiers::resolveAspectRatio(3840, 2160, w, h);
    assert(w == 1920 && h == 1080);

    const FilenameModifiers m2 = FilenameModifiers::parseFilenameModifiers(
        "C:\\clips\\shot!h=720!color_range=limited!reset_first_timestamp=1.mov.openmedia");
    assert(m2.cleanPath == "C:\\clips\\shot.mov");
    assert(m2.height == 720);
    assert(m2.colorRange == openmedia::ColorRange::Limited);
    assert(m2.resetFirstTimestamp);
}

void testErrorDeduplicator() {
    ErrorDeduplicator dedup;
    assert(dedup.shouldLogWarning("fileA", "corrupt_pts", true));
    assert(!dedup.shouldLogWarning("fileA", "corrupt_pts", true));
    assert(dedup.shouldLogWarning("fileA", "corrupt_pts", false));
    assert(dedup.shouldLogWarning("fileB", "corrupt_pts", true));
    dedup.clearFile("fileA");
    assert(dedup.shouldLogWarning("fileA", "corrupt_pts", true));
}

} // namespace

int main() {
    testDualPrmEntryDispatcher();
    testOpenMediaConfigToml();
    testFilenameModifiers();
    testErrorDeduplicator();
    std::cout << "[PASS] test_influx_config_modifiers: Dual-PRM, Config.toml, !modifiers, and ErrorDeduplicator passed.\n";
    return 0;
}
