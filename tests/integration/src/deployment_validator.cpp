/// @file deployment_validator.cpp
/// @brief Implementation of DeploymentValidator for cross-platform MediaCore plugin verification.

#include "deployment_validator.hpp"

#include <array>
#include <cstdint>
#include <cstring>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace openmedia::integration {

namespace {

class ScopedLibrary {
public:
    explicit ScopedLibrary(const std::string& path) noexcept {
        if (path.empty()) {
            return;
        }
#if defined(_WIN32)
        const int wideLen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
        if (wideLen > 0) {
            std::wstring widePath(static_cast<std::size_t>(wideLen), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, widePath.data(), wideLen);
            handle_ = static_cast<void*>(
                LoadLibraryExW(widePath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH));
        }
#else
        handle_ = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
    }

    ~ScopedLibrary() {
        if (handle_ == nullptr) {
            return;
        }
#if defined(_WIN32)
        FreeLibrary(static_cast<HMODULE>(handle_));
#else
        dlclose(handle_);
#endif
    }

    ScopedLibrary(const ScopedLibrary&) = delete;
    ScopedLibrary& operator=(const ScopedLibrary&) = delete;

    [[nodiscard]] bool isLoaded() const noexcept { return handle_ != nullptr; }

    template <typename Fn>
    [[nodiscard]] Fn resolve(const char* symbolName) const noexcept {
        if (handle_ == nullptr || symbolName == nullptr) {
            return nullptr;
        }
#if defined(_WIN32)
        return reinterpret_cast<Fn>(GetProcAddress(static_cast<HMODULE>(handle_), symbolName));
#else
        return reinterpret_cast<Fn>(dlsym(handle_, symbolName));
#endif
    }

private:
    void* handle_{nullptr};
};

bool enumerateImporterFormats(ImportEntryFn importFn,
                              imStdParms* stdParms,
                              int& outFormatCount) {
    outFormatCount = 0;
    for (std::intptr_t idx = 0; idx < 16; ++idx) {
        importer::ImIndFormatRecExt fmtRec{};
        const csSDK_int32 rc = importFn(
            importer::kSelGetIndFormat, stdParms, reinterpret_cast<void*>(idx), &fmtRec);
        if (rc == importer::kImNoErr) {
            ++outFormatCount;
            continue;
        }
        return rc == importer::kImBadFormatIndex && outFormatCount >= 2;
    }
    return outFormatCount >= 2;
}

bool probeImporterLifecycleSelectors(ImportEntryFn importFn, imStdParms* stdParms) {
    importer::ImOpenFileRec8Ext openRec{};
    importer::ImFileInfoRec8Ext infoRec{};
    importer::ImSourceVideoRecExt videoRec{};
    importer::ImImportAudioRec7Ext audioRec{};

    const std::array<std::pair<csSDK_int32, void*>, 4> probes{{
        {importer::kSelOpenFile8, &openRec},
        {importer::kSelGetInfo8, &infoRec},
        {importer::kSelGetSourceVideo, &videoRec},
        {importer::kSelImportAudio7, &audioRec},
    }};
    for (const auto& probe : probes) {
        const csSDK_int32 rc = importFn(probe.first, stdParms, probe.second, nullptr);
        if (rc != importer::kImNoErr && rc != importer::kImUnsupported) {
            return false;
        }
    }
    return importFn(importer::kSelShutdown, stdParms, nullptr, nullptr) == importer::kImNoErr;
}

} // namespace

DeploymentValidationResult DeploymentValidator::validatePluginBinaries(
    const std::string& importerPath,
    const std::string& exporterPath) const {
    DeploymentValidationResult result{};
    ScopedLibrary importerLib(importerPath);
    ScopedLibrary exporterLib(exporterPath);

    const auto importFn = importerLib.resolve<ImportEntryFn>("xImportEntry");
    const auto exportFn = exporterLib.resolve<ExportEntryFn>("xSDKExport");

    result.importerLoaded = importerLib.isLoaded() && importFn != nullptr;
    result.exporterLoaded = exporterLib.isLoaded() && exportFn != nullptr;
    if (!result.importerLoaded || !result.exporterLoaded) {
        result.errorMessage = "Failed to load plugin binary or resolve entry point symbol.";
        return result;
    }

    return validateDirectEntryPoints(importFn, exportFn);
}

DeploymentValidationResult DeploymentValidator::validateDirectEntryPoints(
    ImportEntryFn importFn,
    ExportEntryFn exportFn) const {
    DeploymentValidationResult result{};
    if (importFn == nullptr || exportFn == nullptr) {
        result.errorMessage = "Null entry point function pointer supplied.";
        return result;
    }

    result.importerLoaded = true;
    result.exporterLoaded = true;
    MockPremiereHost host;
    result.importerHandshakeOk = verifyImporterHandshake(
        importFn, host, result.importerPriority,
        result.avoidAudioConform, result.importerFormatCount);
    result.exporterHandshakeOk = verifyExporterHandshake(
        exportFn, host, result.exporterContainerCount, result.exporterExtensions);
    return result;
}

bool DeploymentValidator::verifyImporterHandshake(
    ImportEntryFn importFn,
    MockPremiereHost& host,
    csSDK_int32& outPriority,
    bool& outAvoidConform,
    int& outFormatCount) {
    if (importFn == nullptr) {
        return false;
    }
    importer::ImImportInfoRecExt initInfo{};
    if (importFn(importer::kSelInit, &host.importerStdParms, &initInfo, nullptr) !=
        importer::kImNoErr) {
        return false;
    }
    outPriority = initInfo.priority;
    outAvoidConform = (initInfo.avoidAudioConform == importer::kPrTrueVal);
    if (outPriority != 100 || !outAvoidConform) {
        return false;
    }
    if (!enumerateImporterFormats(importFn, &host.importerStdParms, outFormatCount)) {
        return false;
    }
    return probeImporterLifecycleSelectors(importFn, &host.importerStdParms);
}

bool DeploymentValidator::verifyExporterHandshake(
    ExportEntryFn exportFn,
    MockPremiereHost& host,
    int& outContainerCount,
    std::vector<std::string>& outExtensions) {
    if (exportFn == nullptr) {
        return false;
    }
    outContainerCount = 0;
    outExtensions.clear();
    for (csSDK_int32 idx = 0; idx < 4; ++idx) {
        exporter::ExStartupInfoRecExt startupInfo{};
        startupInfo.exporterIndex = idx;
        const csSDK_int32 rc = exportFn(
            exSelStartup, &host.exporterStdParms, &startupInfo, nullptr);
        if (rc == exporter::kExIterateExporter || rc == exporter::kExIterateExporterDone) {
            if (!startupInfo.fileExtension.empty()) {
                ++outContainerCount;
                outExtensions.push_back(startupInfo.fileExtension);
            }
            if (rc == exporter::kExIterateExporterDone) {
                break;
            }
        }
    }
    prBool isStillSeq = 1;
    const bool lifecycleOk =
        exportFn(exSelBeginInstance, &host.exporterStdParms, nullptr, nullptr) == exporter::kExNoErr &&
        exportFn(kExSelQueryStillSequence, &host.exporterStdParms, &isStillSeq, nullptr) == exporter::kExNoErr &&
        isStillSeq == 0 &&
        exportFn(exSelEndInstance, &host.exporterStdParms, nullptr, nullptr) == exporter::kExNoErr;
    return lifecycleOk && outContainerCount == 3;
}

} // namespace openmedia::integration
