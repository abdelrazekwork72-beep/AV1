/// @file test_deployment_validator.cpp
/// @brief Standalone C++17 integration test for TC-4.1.1 (DeploymentValidator against real xImportEntry & xSDKExport).

#include "deployment_validator.hpp"

#include <cassert>
#include <iostream>

int main(int argc, char** argv) {
    openmedia::integration::DeploymentValidator validator;

    // 1. Missing binary paths must fail gracefully without crashing
    const auto missingRes = validator.validatePluginBinaries(
        "/nonexistent/OpenMedia_Importer.prm",
        "/nonexistent/OpenMedia_Exporter.prm");
    assert(!missingRes.passed());
    assert(!missingRes.importerLoaded);
    assert(!missingRes.exporterLoaded);

    // 2. Null pointer safety check
    const auto nullRes = validator.validateDirectEntryPoints(nullptr, nullptr);
    assert(!nullRes.passed());

    // 3. Direct entry-point handshake verification against REAL xImportEntry & xSDKExport (TC-4.1.1)
    const auto directRes = validator.validateDirectEntryPoints(&xImportEntry, &xSDKExport);
    assert(directRes.passed());
    assert(directRes.importerPriority == 100);
    assert(directRes.avoidAudioConform);
    assert(directRes.importerFormatCount == 7);
    assert(directRes.exporterContainerCount == 3);
    assert(directRes.exporterExtensions.size() == 3);
    assert(directRes.exporterExtensions[0] == "mkv");
    assert(directRes.exporterExtensions[1] == "webm");
    assert(directRes.exporterExtensions[2] == "mp4");

    // 4. Dynamic binary load verification when compiled .prm/.bundle/.so paths are supplied
    if (argc >= 3) {
        const auto binRes = validator.validatePluginBinaries(argv[1], argv[2]);
        assert(binRes.passed());
        std::cout << "[PASS] TC-4.1.1 Dynamic dlopen/LoadLibraryExW verified on "
                  << argv[1] << " & " << argv[2] << std::endl;
    }

    std::cout << "[PASS] TC-4.1.1 DeploymentValidator verified real xImportEntry (7 formats, priority=100, avoidAudioConform=1) "
              << "and xSDKExport (3 containers: mkv, webm, mp4)." << std::endl;
    return 0;
}
