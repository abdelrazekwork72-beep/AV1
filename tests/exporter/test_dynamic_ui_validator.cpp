#include "exporter/dynamic_ui_validator.hpp"

#include <cassert>
#include <iostream>

using namespace openmedia::exporter;

int main() {
    ExParamChangedRecExt rec{};
    rec.changedParamId = "encoderBackend";
    rec.hardwareEnabled = true;
    assert(DynamicUIValidator::validateParamChanged(rec) == kExNoErr);
    assert(rec.rebuildAllParams == 1);

    rec.bitDepth = 9;
    assert(DynamicUIValidator::validateParamChanged(rec) == kExError);

    std::cout << "[PASS] TC-3.1.5 DynamicUIValidator rebuildAllParams & validation passed.\n";
    return 0;
}
