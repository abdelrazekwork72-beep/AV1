#include "exporter/export_param_builder.hpp"

#include <cassert>
#include <iostream>

using namespace openmedia::exporter;

int main() {
    ExporterPresetParams p{};
    csSDK_int32 suiteVer = 0;
    assert(ExportParamBuilder::generateDefaults(p, &suiteVer) == kExNoErr);
    assert(suiteVer == 1001);
    assert(p.version == 1001);

    p.crf = 24;
    const std::string epr = ExportParamBuilder::serializePreset(p);
    ExporterPresetParams loaded{};
    assert(ExportParamBuilder::deserializePreset(epr, loaded));
    assert(loaded.crf == 24 && loaded.version == 1001);

    std::cout << "[PASS] TC-3.1.2 ExportParamBuilder SetParamsVersion(1001) & .epr preset passed.\n";
    return 0;
}
