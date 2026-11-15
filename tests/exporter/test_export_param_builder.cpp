#include "exporter/export_param_builder.hpp"
#include "exporter/exporter_entry.hpp"

#include <cassert>
#include <iostream>
#include <sstream>

using namespace openmedia::exporter;

int main() {
    ExporterPresetParams p{};
    csSDK_int32 suiteVer = 0;
    assert(ExportParamBuilder::generateDefaults(p, &suiteVer) == kExNoErr);
    assert(suiteVer == 1001);
    assert(p.version == 1001);
    assert(p.colorMetadata == ExportColorMetadata::BT709);

    p.crf = 24;
    p.colorMetadata = ExportColorMetadata::BT2100_PQ;
    const std::string epr = ExportParamBuilder::serializePreset(p);
    ExporterPresetParams loaded{};
    assert(ExportParamBuilder::deserializePreset(epr, loaded));
    assert(loaded.crf == 24 && loaded.version == 1001);
    assert(loaded.colorMetadata == ExportColorMetadata::BT2100_PQ);

    p.colorMetadata = ExportColorMetadata::BT2100_HLG;
    assert(ExportParamBuilder::parseColorMetadata(ExportParamBuilder::colorMetadataToString(p.colorMetadata)) ==
           ExportColorMetadata::BT2100_HLG);
    p.colorMetadata = ExportColorMetadata::Sony_SLog3;
    assert(ExportParamBuilder::parseColorMetadata(ExportParamBuilder::colorMetadataToString(p.colorMetadata)) ==
           ExportColorMetadata::Sony_SLog3);

    const auto args = ExporterEntry::buildPopupLaunchArgs(0x12345ULL);
    assert(args.size() == 3 && args[0] == "--popup" && args[1] == "--owner");

    PopupIpcMessage req{};
    req.configTomlPath = "C:/Users/ARZ/AppData/Roaming/OpenMedia/Config.toml";
    req.gpuStatus = {"nvdec_av1", "av1_nvenc", true};
    req.preset = p;

    const std::string wireLine = PopupIpcBridge::serializeJsonLine(req);
    assert(wireLine.rfind("json:", 0) == 0);

    std::istringstream simulatedStdIn(wireLine);
    std::ostringstream simulatedStdOut;
    PopupIpcMessage resp{};
    assert(ExporterEntry::exchangePopupSettings(simulatedStdIn, simulatedStdOut, req, resp));
    assert(simulatedStdOut.str() == wireLine);
    assert(resp.configTomlPath == req.configTomlPath);
    assert(resp.gpuStatus.activeDecoder == "nvdec_av1");
    assert(resp.gpuStatus.activeEncoder == "av1_nvenc");
    assert(resp.gpuStatus.hardwareAvailable);
    assert(resp.preset.colorMetadata == ExportColorMetadata::Sony_SLog3);

    std::cout << "[PASS] TC-3.1.2 ExportParamBuilder & PopupIpcBridge json: IPC + BT.2100/S-Log3 passed.\n";
    return 0;
}
