#include "importer/audio_conform_bypass.hpp"

#include <cassert>
#include <iostream>

using namespace openmedia::importer;

int main() {
    ImImportInfoRecExt initInfo{};
    ImFileInfoRec8Ext fileInfo{};
    assert(AudioConformBypass::configureInit(&initInfo) == kImNoErr);
    assert(AudioConformBypass::configureFileInfo(&fileInfo) == kImNoErr);

    assert(initInfo.avoidAudioConform == kPrTrueVal);
    assert(initInfo.canProvidePeakAudio == kPrTrueVal);
    assert((fileInfo.accessModes & kRandomAccessImportFlag) != 0);
    assert(AudioConformBypass::sampleToByteOffset(48000, 48000, 128000) == 16000);

    std::cout << "[PASS] TC-2.4.2 AudioConformBypass zero-conform & random-access passed.\n";
    return 0;
}
