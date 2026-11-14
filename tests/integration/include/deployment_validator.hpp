#pragma once
/// @file deployment_validator.hpp
/// @brief Dynamic MediaCore plugin binary loader and selector handshake validator (US-4.1.0 Unit 1).

#include "mock_premiere_suites.hpp"
#include "importer/importer_entry.hpp"
#ifdef PREMPLUGENTRY
#undef PREMPLUGENTRY
#endif
#include "exporter/exporter_entry.hpp"

#include <string>
#include <vector>

namespace openmedia::integration {

/// @brief Result of validating OpenMedia_Importer and OpenMedia_Exporter binaries / entry points.
struct DeploymentValidationResult {
    bool importerLoaded{false};
    bool exporterLoaded{false};
    bool importerHandshakeOk{false};
    bool exporterHandshakeOk{false};
    csSDK_int32 importerPriority{0};
    bool avoidAudioConform{false};
    int importerFormatCount{0};
    int exporterContainerCount{0};
    std::vector<std::string> exporterExtensions;
    std::string errorMessage;

    /// @brief Returns true when all binary loads and selector handshakes satisfy AC-1 / TC-4.1.1.
    [[nodiscard]] bool passed() const noexcept {
        return importerLoaded && exporterLoaded && importerHandshakeOk &&
               exporterHandshakeOk && importerPriority == 100 &&
               avoidAudioConform && importerFormatCount >= 2 &&
               exporterContainerCount == 3;
    }
};

/// @brief Validates OpenMedia_Importer and OpenMedia_Exporter binaries via OS dynamic loading.
class DeploymentValidator {
public:
    /// @brief Dynamically loads both plugin binaries (.prm/.bundle/.so) and executes selector handshakes.
    [[nodiscard]] DeploymentValidationResult validatePluginBinaries(
        const std::string& importerPath,
        const std::string& exporterPath) const;

    /// @brief Validates already-resolved xImportEntry and xSDKExport function pointers in-process.
    [[nodiscard]] DeploymentValidationResult validateDirectEntryPoints(
        ImportEntryFn importFn,
        ExportEntryFn exportFn) const;

    /// @brief Verifies the real importer selector sequence (imInit, imGetIndFormat, imShutdown, etc.).
    [[nodiscard]] static bool verifyImporterHandshake(
        ImportEntryFn importFn,
        MockPremiereHost& host,
        csSDK_int32& outPriority,
        bool& outAvoidConform,
        int& outFormatCount);

    /// @brief Verifies the real exporter selector sequence (exSelStartup iteration across MKV/WebM/MP4, instance lifecycle).
    [[nodiscard]] static bool verifyExporterHandshake(
        ExportEntryFn exportFn,
        MockPremiereHost& host,
        int& outContainerCount,
        std::vector<std::string>& outExtensions);
};

} // namespace openmedia::integration
