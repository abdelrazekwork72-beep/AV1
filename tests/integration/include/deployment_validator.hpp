#pragma once
/// @file deployment_validator.hpp
/// @brief Dynamic MediaCore plugin binary loader and Dual-PRM selector handshake validator.

#include "mock_premiere_suites.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace openmedia::integration {

/// @brief Priority tier for Dual-PRM MediaCore importer registration.
enum class ImporterPriorityTier : std::uint8_t {
    Normal = 0x00,
    High = 0x01
};

/// @brief Canonical PiPL match names for Normal and High-Priority importers.
inline constexpr const char* kNormalImporterPiplName = "OpenMedia Importer";
inline constexpr const char* kHighPriorityImporterPiplName = "OpenMedia Importer ";

/// @brief Validation state for a single importer binary variant (Normal or HP).
struct ImporterVariantResult {
    bool loaded{false};
    bool handshakeOk{false};
    csSDK_int32 priority{0};
    ImporterPriorityTier priorityTier{ImporterPriorityTier::Normal};
    bool avoidAudioConform{false};
    std::string piplName;
    bool piplValid{false};
};

/// @brief Result of validating single or Dual-PRM plugin binaries and exporter.
struct DeploymentValidationResult {
    bool importerLoaded{false};
    bool exporterLoaded{false};
    bool importerHandshakeOk{false};
    bool exporterHandshakeOk{false};
    csSDK_int32 importerPriority{0};
    bool avoidAudioConform{false};
    int exporterContainerCount{0};
    bool dualPrmMode{false};
    ImporterVariantResult normalImporter{};
    ImporterVariantResult hpImporter{};
    std::string errorMessage;

    /// @brief Returns true when all required binary loads, PiPL checks, and handshakes succeeded.
    [[nodiscard]] bool passed() const noexcept {
        if (dualPrmMode) {
            return normalImporter.loaded && normalImporter.handshakeOk &&
                   normalImporter.piplValid &&
                   normalImporter.priorityTier == ImporterPriorityTier::Normal &&
                   hpImporter.loaded && hpImporter.handshakeOk &&
                   hpImporter.piplValid &&
                   hpImporter.priorityTier == ImporterPriorityTier::High &&
                   exporterLoaded && exporterHandshakeOk &&
                   exporterContainerCount == 3;
        }
        return importerLoaded && exporterLoaded && importerHandshakeOk &&
               exporterHandshakeOk && importerPriority == 100 &&
               avoidAudioConform && exporterContainerCount == 3;
    }
};

/// @brief Validates OpenMedia_Importer, OpenMedia_ImporterHP, and OpenMedia_Exporter binaries.
class DeploymentValidator {
public:
    /// @brief Dynamically loads single importer and exporter binaries and executes handshakes.
    [[nodiscard]] DeploymentValidationResult validatePluginBinaries(
        const std::string& importerPath,
        const std::string& exporterPath) const;

    /// @brief Dynamically loads Dual-PRM (Normal + HP) and exporter binaries and verifies both.
    [[nodiscard]] DeploymentValidationResult validateDualPluginBinaries(
        const std::string& normalImporterPath,
        const std::string& hpImporterPath,
        const std::string& exporterPath) const;

    /// @brief Validates already-resolved single-PRM entry point function pointers in-process.
    [[nodiscard]] DeploymentValidationResult validateDirectEntryPoints(
        ImportEntryFn importFn,
        ExportEntryFn exportFn) const;

    /// @brief Validates Dual-PRM (Normal 0x00 + HP 0x01) entry points and PiPL names in-process.
    [[nodiscard]] DeploymentValidationResult validateDualDirectEntryPoints(
        ImportEntryFn normalImportFn,
        ImportEntryFn hpImportFn,
        ExportEntryFn exportFn,
        const std::string& normalPiplName = kNormalImporterPiplName,
        const std::string& hpPiplName = kHighPriorityImporterPiplName) const;

    /// @brief Verifies PiPL name matches the expected priority tier (trailing space for HP).
    [[nodiscard]] static bool verifyPiplName(
        ImporterPriorityTier tier,
        const std::string& piplName) noexcept;

    /// @brief Verifies the importer selector sequence (imInit, imGetIndFormat, etc.).
    [[nodiscard]] static bool verifyImporterHandshake(
        ImportEntryFn importFn,
        MockPremiereHost& host,
        csSDK_int32& outPriority,
        bool& outAvoidConform);

    /// @brief Verifies the exporter selector sequence (exSelStartup iteration, instance lifecycle).
    [[nodiscard]] static bool verifyExporterHandshake(
        ExportEntryFn exportFn,
        MockPremiereHost& host,
        int& outContainerCount);
};

} // namespace openmedia::integration
