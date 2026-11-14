#pragma once
/// @file deployment_validator.hpp
/// @brief Dynamic MediaCore plugin binary loader and selector handshake validator.

#include "mock_premiere_suites.hpp"

#include <string>
#include <vector>

namespace openmedia::integration {

/// @brief Result of validating a single plugin binary or entry point pair.
struct DeploymentValidationResult {
    bool importerLoaded{false};
    bool exporterLoaded{false};
    bool importerHandshakeOk{false};
    bool exporterHandshakeOk{false};
    csSDK_int32 importerPriority{0};
    bool avoidAudioConform{false};
    int exporterContainerCount{0};
    std::string errorMessage;

    /// @brief Returns true when all binary loads and selector handshakes succeeded.
    [[nodiscard]] bool passed() const noexcept {
        return importerLoaded && exporterLoaded && importerHandshakeOk &&
               exporterHandshakeOk && importerPriority == 100 &&
               avoidAudioConform && exporterContainerCount == 3;
    }
};

/// @brief Validates OpenMedia_Importer and OpenMedia_Exporter binaries via OS dynamic loading.
class DeploymentValidator {
public:
    /// @brief Dynamically loads both plugin binaries and executes selector handshakes.
    /// @param importerPath Path to OpenMedia_Importer.prm/.bundle/.so.
    /// @param exporterPath Path to OpenMedia_Exporter.prm/.bundle/.so.
    /// @return Aggregated validation result with diagnostic details.
    [[nodiscard]] DeploymentValidationResult validatePluginBinaries(
        const std::string& importerPath,
        const std::string& exporterPath) const;

    /// @brief Validates already-resolved entry point function pointers in-process.
    /// @param importFn Resolved xImportEntry function pointer.
    /// @param exportFn Resolved xSDKExport function pointer.
    /// @return Aggregated validation result.
    [[nodiscard]] DeploymentValidationResult validateDirectEntryPoints(
        ImportEntryFn importFn,
        ExportEntryFn exportFn) const;

    /// @brief Verifies the importer selector sequence (imInit, imGetIndFormat, etc.).
    /// @param importFn Importer entry point function pointer.
    /// @param host Mock Premiere host environment.
    /// @param outPriority Populated with advertised importer priority (expected 100).
    /// @param outAvoidConform Populated with avoidAudioConform flag (expected true).
    /// @return True if all importer selectors succeed.
    [[nodiscard]] static bool verifyImporterHandshake(
        ImportEntryFn importFn,
        MockPremiereHost& host,
        csSDK_int32& outPriority,
        bool& outAvoidConform);

    /// @brief Verifies the exporter selector sequence (exSelStartup iteration, instance lifecycle).
    /// @param exportFn Exporter entry point function pointer.
    /// @param host Mock Premiere host environment.
    /// @param outContainerCount Populated with number of iterated containers (expected 3).
    /// @return True if all exporter selectors succeed.
    [[nodiscard]] static bool verifyExporterHandshake(
        ExportEntryFn exportFn,
        MockPremiereHost& host,
        int& outContainerCount);
};

} // namespace openmedia::integration
