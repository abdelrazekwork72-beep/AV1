#pragma once
// Clean-room minimal C++17 mock header for CI/testing - zero proprietary Adobe SDK code.
#include "PrSDKTypes.h"

struct exParamValues {
    csSDK_int32 intValue = 0;
    double floatValue = 0.0;
};

struct exNewParamInfo {
    const char* identifier = nullptr;
    exParamValues paramValues{};
};

struct PrSDKExportParamSuite {
    prSuiteError (*AddMultiGroup)(csSDK_uint32 exporterPluginID) = nullptr;
    prSuiteError (*AddParamGroup)(csSDK_uint32 exporterPluginID, csSDK_int32 multiGroupIndex, const char* groupName, const char* identifier) = nullptr;
    prSuiteError (*AddParam)(csSDK_uint32 exporterPluginID, csSDK_int32 multiGroupIndex, const char* groupIdentifier, const exNewParamInfo* paramInfo) = nullptr;
    prSuiteError (*GetParamValue)(csSDK_uint32 exporterPluginID, csSDK_int32 multiGroupIndex, const char* identifier, exParamValues* outValue) = nullptr;
    prSuiteError (*ChangeParam)(csSDK_uint32 exporterPluginID, csSDK_int32 multiGroupIndex, const char* identifier, const exParamValues* newValue) = nullptr;
};
