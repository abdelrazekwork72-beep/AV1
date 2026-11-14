#include "exporter/exporter_entry.hpp"
#include <cassert>
#include <iostream>
using namespace openmedia::exporter;
int main() {
    ExStartupInfoRecExt rec{};
    rec.exporterIndex = 0;
    assert(xSDKExport(exSelStartup, nullptr, &rec, nullptr) == kExIterateExporter);
    assert(rec.fileType == kExporterMkvFourCC && rec.fileExtension == "mkv" && rec.isSequencePreviewFormat == 1);
    rec.exporterIndex = 1;
    assert(xSDKExport(exSelStartup, nullptr, &rec, nullptr) == kExIterateExporter);
    assert(rec.fileExtension == "webm");
    rec.exporterIndex = 2;
    assert(xSDKExport(exSelStartup, nullptr, &rec, nullptr) == kExIterateExporterDone);
    assert(rec.fileExtension == "mp4" && rec.canMatchSource == 1 && rec.canEmbedCaptions == 1);
    std::cout << "[PASS] TC-3.1.1 ExporterEntry xSDKExport multi-container iteration passed.\n";
    return 0;
}
