/// @file test_deployment_validator.cpp
/// @brief Standalone C++17 integration test for TC-4.1.1 (Dual-PRM DeploymentValidator).

#undef NDEBUG
#include "deployment_validator.hpp"
#include "importer/importer_entry.hpp"
#include "exporter/exporter_entry.hpp"

#include <cassert>
#include <iostream>

int main() {
    openmedia::integration::DeploymentValidator validator;

    // 1. Missing binary paths must fail gracefully without crashing
    const auto missingRes = validator.validatePluginBinaries(
        "/nonexistent/OpenMedia_Importer.prm",
        "/nonexistent/OpenMedia_Exporter.prm");
    assert(!missingRes.passed());
    assert(!missingRes.importerLoaded);
    assert(!missingRes.exporterLoaded);

    const auto missingDualRes = validator.validateDualPluginBinaries(
        "/nonexistent/OpenMedia_Importer.prm",
        "/nonexistent/OpenMedia_ImporterHP.prm",
        "/nonexistent/OpenMedia_Exporter.prm");
    assert(!missingDualRes.passed());
    assert(missingDualRes.dualPrmMode);
    assert(!missingDualRes.normalImporter.loaded);
    assert(!missingDualRes.hpImporter.loaded);

    // 2. Direct single-PRM entry-point handshake verification (TC-4.1.1)
    const auto directRes = validator.validateDirectEntryPoints(&xImportEntry, &xSDKExport);
    assert(directRes.passed());
    assert(directRes.importerPriority == 100);
    assert(directRes.avoidAudioConform);
    assert(directRes.exporterContainerCount == 3);

    // 3. Dual-PRM (Normal 0x00 + HP 0x01) entry-point & PiPL verification
    const auto dualRes = validator.validateDualDirectEntryPoints(
        &xImportEntry, &xImportEntry, &xSDKExport,
        "OpenMedia Importer", "OpenMedia Importer ");
    assert(dualRes.passed());
    assert(dualRes.normalImporter.piplValid);
    assert(dualRes.normalImporter.priorityTier == openmedia::integration::ImporterPriorityTier::Normal);
    assert(dualRes.hpImporter.piplValid);
    assert(dualRes.hpImporter.priorityTier == openmedia::integration::ImporterPriorityTier::High);
    assert(dualRes.exporterContainerCount == 3);

    // 4. Negative test: HP PiPL without trailing space must be rejected
    const auto badHpPipl = validator.validateDualDirectEntryPoints(
        &xImportEntry, &xImportEntry, &xSDKExport,
        "OpenMedia Importer", "OpenMedia Importer");
    assert(!badHpPipl.passed());
    assert(!badHpPipl.hpImporter.piplValid);

    std::cout << "[PASS] TC-4.1.1 Dual-PRM DeploymentValidator verified Normal (0x00), HP (0x01 + trailing space), and Exporter handshakes.\n";
    return 0;
}
