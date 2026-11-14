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
    fs::create_directories(tempDir);

    // Seed whitelisted stale AME files AND an unrelated user preset that MUST NOT be deleted
    { std::ofstream(tempDir / "Placeholder Preset.epr") << "stale"; }
    { std::ofstream(tempDir / "batch.xml") << "<batch/>"; }
    { std::ofstream(tempDir / "UserCustomPreset.epr") << "keep"; }

    openmedia::integration::AmeQueueValidator validator;
    const auto runner = [](const openmedia::integration::AmeExportJob& job,
                           std::vector<std::uint8_t>& outBytes) -> bool {
        if (job.containerExt == ".mkv" || job.containerExt == ".webm") {
            outBytes = {0x1A, 0x45, 0xDF, 0xA3, 0x01};
        } else if (job.containerExt == ".opus") {
            outBytes = {'O', 'g', 'g', 'S', 0x00};
        } else if (job.containerExt == ".flac") {
            outBytes = {'f', 'L', 'a', 'C', 0x00};
        }
        return !outBytes.empty();
    };

    const auto report = validator.runBatchQueueValidation(tempDir, runner);
    assert(report.purgedFileCount == 2);
    assert(!fs::exists(tempDir / "Placeholder Preset.epr"));
    assert(!fs::exists(tempDir / "batch.xml"));
    assert(fs::exists(tempDir / "UserCustomPreset.epr"));
    assert(report.passed());

    fs::remove_all(tempDir);
    std::cout << "[PASS] TC-4.1.3 AmeQueueValidator purged 2 stale caches and passed 5-job batch.\n";
    return 0;
}
