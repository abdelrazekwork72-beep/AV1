#include "exporter/dynamic_ui_validator.hpp"

namespace openmedia::exporter {

csSDK_int32 DynamicUIValidator::validateParamChanged(ExParamChangedRecExt& rec) noexcept {
    if (rec.width < 16 || rec.height < 16 || rec.width > 16384 || rec.height > 16384) {
        return kExError;
    }
    if (rec.bitDepth != 8 && rec.bitDepth != 10 && rec.bitDepth != 12) {
        return kExError;
    }
    if (rec.changedParamId == "encoderBackend" || rec.changedParamId == "chromaSubsampling") {
        rec.rebuildAllParams = 1;
    }
    return kExNoErr;
}

} // namespace openmedia::exporter
