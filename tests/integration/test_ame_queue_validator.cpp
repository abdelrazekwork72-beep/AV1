/// @file test_ame_queue_validator.cpp
/// @brief Standalone C++17 integration test for TC-4.1.3 (AmeQueueValidator).

#include "ame_queue_validator.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

int main() {
    namespace fs = std::filesystem;
    const fs::path tempDir = fs::temp_directory_path() / "openmedia_ame_test_cache";
    fs::remove_all(tempDir);
    fs::create_directories(tempDir);

    // Seed whitelisted stale AME files AND an unrelated user preset that MUST NOT be deleted
    { std::ofstream(tempDir / "Placeholder Preset.epr") << "stale"; }
    { std::ofstream(tempDir / "batch.xml") << "<batch/>"; }
    { std::ofstream(tempDir / "UserCustomPreset.epr") << "keep"; }

    openmedia::integration::AmeQueueValidator validator;
    const auto report = validator.runDefaultBatchPipeline(tempDir);

    assert(report.purgedFileCount == 2);
    assert(!fs::exists(tempDir / "Placeholder Preset.epr"));
    assert(!fs::exists(tempDir / "batch.xml"));
    assert(fs::exists(tempDir / "UserCustomPreset.epr"));
    assert(report.completedJobs == 5);
    assert(report.failedJobs == 0);
    assert(report.allContainersValid);
    assert(report.demuxerProbesPassed);
    assert(report.memoryDeltaMb < 50.0);
    assert(report.passed());

    fs::remove_all(tempDir);
    std::cout << "[PASS] TC-4.1.3 AmeQueueValidator purged 2 stale caches, verified 5 containers with CoreDemuxer, "
              << "RSS delta=" << report.memoryDeltaMb << "MB (< 50MB)." << std::endl;
    return 0;
}
