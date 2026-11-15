/// @file test_importer_registration.cpp
/// @brief Standalone C++17 unit test (TC-2.1.1) for xImportEntry registration and format discovery.

#include "importer/importer_entry.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

using namespace openmedia::importer;

namespace {

void testImInitCapabilities() {
    imStdParms stdParms{};
    ImImportInfoRecExt info{};

    const csSDK_int32 err = xImportEntry(kSelInit, &stdParms, &info, nullptr);
    assert(err == kImNoErr);
    assert(info.canOpen == kPrTrueVal);
    assert(info.hasSetup == kPrTrueVal);
    assert(info.priority == 100);
    assert(info.avoidAudioConform == 1);
    assert(info.canProvidePeakAudio == 1);

    const csSDK_int32 nullErr = xImportEntry(kSelInit, &stdParms, nullptr, nullptr);
    assert(nullErr == kImMemErr);
}

void testImGetIndFormatRegistration() {
    imStdParms stdParms{};
    const std::array<const char*, 7> expectedExts = {
        "mkv", "webm", "ogg", "flv", "avi", "mp4", "mov"
    };

    for (std::size_t i = 0; i < expectedExts.size(); ++i) {
        ImIndFormatRecExt rec{};
        void* indexPtr = reinterpret_cast<void*>(static_cast<std::intptr_t>(i));
        const csSDK_int32 err = xImportEntry(kSelGetIndFormat, &stdParms, indexPtr, &rec);
        assert(err == kImNoErr);
        assert(rec.filetype == kOpenMediaImporterFourCC);
        assert(std::strcmp(rec.extension, expectedExts[i]) == 0);
        assert(std::strlen(rec.formatName) > 0);
        assert(std::strlen(rec.formatShortName) > 0);
    }

    ImIndFormatRecExt oobRec{};
    assert(xImportEntry(kSelGetIndFormat, &stdParms, reinterpret_cast<void*>(static_cast<std::intptr_t>(39)), &oobRec) == kImBadFormatIndex);
    assert(xImportEntry(kSelGetIndFormat, &stdParms, reinterpret_cast<void*>(static_cast<std::intptr_t>(-1)), &oobRec) == kImBadFormatIndex);
    assert(xImportEntry(kSelGetIndFormat, &stdParms, reinterpret_cast<void*>(static_cast<std::intptr_t>(0)), nullptr) == kImMemErr);
}

void testSelectorDispatch() {
    imStdParms stdParms{};
    assert(xImportEntry(kSelShutdown, &stdParms, nullptr, nullptr) == kImNoErr);
    assert(xImportEntry(9999, &stdParms, nullptr, nullptr) == kImUnsupported);
}

} // namespace

int main() {
    testImInitCapabilities();
    testImGetIndFormatRegistration();
    testSelectorDispatch();
    std::cout << "TC-2.1.1 PASSED: ImporterEntry & xImportEntry verified (7 formats, priority=100, avoidAudioConform=1, canProvidePeakAudio=1)." << std::endl;
    return 0;
}
