#pragma once
/// @file ame_queue_validator.hpp
/// @brief Adobe Media Encoder (AME) preset cache purge and 5-job batch queue validator.

#include "mock_premiere_suites.hpp"

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
    std::string codecName;
};

/// @brief Callback executing a single export job and returning output container bytes.
using AmeJobRunnerFn = std::function<bool(const AmeExportJob& job,
                                          std::vector<std::uint8_t>& outBytes)>;

/// @brief Aggregated result of AME cache purge and 5-job batch validation.
struct AmeBatchReport {
    std::size_t purgedFileCount{0};
    std::size_t completedJobs{0};
    std::size_t failedJobs{0};
    double memoryDeltaMb{0.0};
    bool allContainersValid{false};

    /// @brief Returns true when all 5 jobs pass with 0 errors and < 50MB memory delta.
    [[nodiscard]] bool passed() const noexcept {
        return completedJobs == 5 && failedJobs == 0 &&
               allContainersValid && memoryDeltaMb < 50.0;
    }
};

/// @brief Purges stale AME preset caches and validates 5-job multi-container batch queues.
class AmeQueueValidator {
public:
    /// @brief Safely deletes only whitelisted "Placeholder Preset.epr" and "batch.xml" files.
    /// @param cacheDir Validated AME preset cache directory.
    /// @return Number of whitelisted cache files removed.
    [[nodiscard]] static std::size_t purgeAmePresetCache(
        const std::filesystem::path& cacheDir);

    /// @brief Validates container magic bytes for MKV/WebM (EBML), Ogg/Opus, or fLaC.
    /// @param ext File extension (".mkv", ".webm", ".opus", ".flac").
    /// @param bytes Serialized container header bytes.
    /// @return True if container signature matches expected format.
    [[nodiscard]] static bool verifyContainerIntegrity(
        const std::string& ext,
        const std::vector<std::uint8_t>& bytes) noexcept;

    /// @brief Purges stale caches and runs the 5 sequential multi-container batch jobs.
    /// @param cacheDir AME preset cache directory.
    /// @param runner Callback executing each DoMultiPassExportLoop job.
    /// @return Batch validation report.
    [[nodiscard]] AmeBatchReport runBatchQueueValidation(
        const std::filesystem::path& cacheDir,
        const AmeJobRunnerFn& runner) const;

    /// @brief Returns the canonical 5-job AME test matrix (MKV AV1, WebM VP9, MKV 4:4:4, Opus, FLAC).
    [[nodiscard]] static std::vector<AmeExportJob> defaultBatchMatrix();
};

} // namespace openmedia::integration
