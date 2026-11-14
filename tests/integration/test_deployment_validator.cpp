/// @file test_deployment_validator.cpp
/// @brief Standalone C++17 integration test for TC-4.1.1 (DeploymentValidator).

#include "deployment_validator.hpp"

#include <cassert>
#include <iostream>

namespace {

csSDK_int32 mockImportEntry(csSDK_int32 selector,
                            imStdParms* /*stdParms*/,
                            void* param1,
                            void* /*param2*/) {
    if (selector == 0 && param1 != nullptr) {
        auto* flags = static_cast<csSDK_int32*>(param1);
        flags[0] = 100; // priority = 100
        flags[1] = 1;   // avoidAudioConform = kPrTrue
    }
    return 0;
}

csSDK_int32 mockExportEntry(csSDK_int32 selector,
                            exportStdParms* /*stdParms*/,
                            void* param1,
                            void* /*param2*/) {
    if (selector == 0 && param1 != nullptr) {
        const int idx = *static_cast<const int*>(param1);
        return (idx < 3) ? 0 : 1; // 3 containers (MKV, WebM, MP4) then Done
    }
    return 0;
}

} // namespace

int main() {
    openmedia::integration::DeploymentValidator validator;

    // 1. Missing binary paths must fail gracefully without crashing
    const auto missingRes = validator.validatePluginBinaries(
        "/nonexistent/OpenMedia_Importer.prm",
        "/nonexistent/OpenMedia_Exporter.prm");
    assert(!missingRes.passed());
    assert(!missingRes.importerLoaded);
    assert(!missingRes.exporterLoaded);

    // 2. Direct entry-point handshake verification (TC-4.1.1)
    const auto directRes = validator.validateDirectEntryPoints(
        &mockImportEntry, &mockExportEntry);
    assert(directRes.passed());
    assert(directRes.importerPriority == 100);
    assert(directRes.avoidAudioConform);
    assert(directRes.exporterContainerCount == 3);

    std::cout << "[PASS] TC-4.1.1 DeploymentValidator handshake & loader tests passed.\n";
    return 0;
}
