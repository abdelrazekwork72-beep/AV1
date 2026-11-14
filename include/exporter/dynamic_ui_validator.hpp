#pragma once
/// @file dynamic_ui_validator.hpp
/// @brief Dynamic parameter validation (exSelValidateParamChanged) and UI rebuild signaling.

#include "exporter/exporter_types.hpp"

namespace openmedia::exporter {

/// @brief Validates parameter changes and signals rebuildAllParams when hardware/chroma toggles.
class DynamicUIValidator {
public:
    /// @brief Validate changed parameter and set rebuildAllParams = kPrTrue when UI layout changes.
    static csSDK_int32 validateParamChanged(ExParamChangedRecExt& rec) noexcept;
};

} // namespace openmedia::exporter
