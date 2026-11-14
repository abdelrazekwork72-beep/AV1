/// @file test_deployment_validator.cpp
/// @brief Standalone C++17 integration test for TC-4.1.1 (DeploymentValidator) with real xImportEntry & xSDKExport.

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

    // 2. Direct entry-point handshake verification against real xImportEntry & xSDKExport (TC-4.1.1)
    const auto directRes = validator.validateDirectEntryPoints(
        &xImportEntry, &xSDKExport);
    assert(directRes.passed());
    assert(directRes.importerPriority == 100);
    assert(directRes.avoidAudioConform);
    assert(directRes.exporterContainerCount == 3);

    std::cout << "[PASS] TC-4.1.1 DeploymentValidator verified real xImportEntry & xSDKExport handshakes.\n";
    return 0;
}
