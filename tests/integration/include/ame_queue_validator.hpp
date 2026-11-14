#pragma once
/// @file ame_queue_validator.hpp
/// @brief Adobe Media Encoder (AME) preset cache purge and 5-job batch queue validator (US-4.1.0 Unit 3).

#include "mock_premiere_suites.hpp"
#include "exporter/push_render_loop.hpp"
#include "openmedia/demuxer.hpp"
#include "openmedia/muxer.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace openmedia::integration {

/// @brief Specification of a single AME batch export job.
struct AmeExportJob {
    std::string name;
    std::string containerExt;
    std::string formatShortName;
    std::string codecName;
};

/// @brief Callback executing a single export job and writing output container bytes + file.
using AmeJobRunnerFn = std::function<bool(const AmeExportJob& job,
                                          const std::filesystem::path& outputPath,
                                          std::vector<std::uint8_t>& outBytes)>;

/// @brief Aggregated result of AME cache purge and 5-job batch validation.
struct AmeBatchReport {
    std::size_t purgedFileCount{0};
    std::size_t completedJobs{0};
    std::size_t failedJobs{0};
    double memoryDeltaMb{0.0};
    bool allContainersValid{false};
    bool demuxerProbesPassed{false};

    /// @brief Returns true when all 5 jobs pass with 0 errors, CoreDemuxer integrity, and < 50MB memory delta.
    [[nodiscard]] bool passed() const noexcept {
        return completedJobs == 5 && failedJobs == 0 &&
               allContainersValid && demuxerProbesPassed &&
               memoryDeltaMb < 50.0;
    }
};

/// @brief Purges stale AME preset caches and validates 5-job multi-container batch queues.
class AmeQueueValidator {
public:
    /// @brief Safely deletes only whitelisted "Placeholder Preset.epr" and "batch.xml" files (TM-4.1.1 / CWE-22).
    [[nodiscard]] static std::size_t purgeAmePresetCache(
        const std::filesystem::path& cacheDir);

    /// @brief Validates container magic bytes for MKV/WebM (EBML), MP4 (ftyp), Ogg/Opus, or fLaC.
    [[nodiscard]] static bool verifyContainerIntegrity(
        const std::string& ext,
        const std::vector<std::uint8_t>& bytes) noexcept;

    /// @brief Measures current process resident memory in megabytes (Linux /proc/self/statm, Windows, macOS).
    [[nodiscard]] static double measureProcessRssMb() noexcept;

    /// @brief Purges stale caches and runs the 5 sequential multi-container batch jobs.
    [[nodiscard]] AmeBatchReport runBatchQueueValidation(
        const std::filesystem::path& cacheDir,
        const AmeJobRunnerFn& runner) const;

    /// @brief Runs the default 5-job pipeline using PushRenderLoop, CoreMuxer, and CoreDemuxer verification.
    [[nodiscard]] AmeBatchReport runDefaultBatchPipeline(
        const std::filesystem::path& cacheDir) const;

    /// @brief Returns the canonical 5-job AME test matrix (MKV AV1, WebM VP9, MKV 4:4:4 Profile 2, Opus, FLAC).
    [[nodiscard]] static std::vector<AmeExportJob> defaultBatchMatrix();
};

} // namespace openmedia::integration
