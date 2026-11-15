/// @file deployment_validator.cpp
/// @brief Implementation of DeploymentValidator for Dual-PRM MediaCore plugin verification.

#include "deployment_validator.hpp"

#include <array>
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
#if defined(_WIN32)
        if (!path.empty()) {
            const int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
            if (wlen > 0) {
                std::wstring wpath(static_cast<std::size_t>(wlen), L'\0');
                MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wpath.data(), wlen);
                handle_ = LoadLibraryExW(wpath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
            }
        }
#else
        if (!path.empty()) {
            handle_ = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        }
#endif
    }

    ~ScopedLibrary() {
        if (!handle_) {
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
        if (!handle_ || !symbolName) {
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

ImporterVariantResult evaluateImporterVariant(
    ImportEntryFn importFn,
    MockPremiereHost& host,
    ImporterPriorityTier tier,
    const std::string& piplName) {
    ImporterVariantResult variant{};
    variant.loaded = (importFn != nullptr);
    variant.priorityTier = tier;
    variant.piplName = piplName;
    variant.piplValid = DeploymentValidator::verifyPiplName(tier, piplName);
    if (!variant.loaded) {
        return variant;
    }
    variant.handshakeOk = DeploymentValidator::verifyImporterHandshake(
        importFn, host, variant.priority, variant.avoidAudioConform);
    return variant;
}

} // namespace

bool DeploymentValidator::verifyPiplName(
    ImporterPriorityTier tier,
    const std::string& piplName) noexcept {
    if (tier == ImporterPriorityTier::Normal) {
        return piplName == kNormalImporterPiplName;
    }
    return piplName == kHighPriorityImporterPiplName;
}

DeploymentValidationResult DeploymentValidator::validatePluginBinaries(
    const std::string& importerPath,
    const std::string& exporterPath) const {
    DeploymentValidationResult result{};
    ScopedLibrary importerLib(importerPath);
    ScopedLibrary exporterLib(exporterPath);

    auto importFn = importerLib.resolve<ImportEntryFn>("xImportEntry");
    auto exportFn = exporterLib.resolve<ExportEntryFn>("xSDKExport");

    result.importerLoaded = importerLib.isLoaded() && importFn != nullptr;
    result.exporterLoaded = exporterLib.isLoaded() && exportFn != nullptr;
    if (!result.importerLoaded || !result.exporterLoaded) {
        result.errorMessage = "Failed to load plugin binary or resolve entry point symbol.";
        return result;
    }

    return validateDirectEntryPoints(importFn, exportFn);
}

DeploymentValidationResult DeploymentValidator::validateDualPluginBinaries(
    const std::string& normalImporterPath,
    const std::string& hpImporterPath,
    const std::string& exporterPath) const {
    DeploymentValidationResult result{};
    result.dualPrmMode = true;

    ScopedLibrary normalLib(normalImporterPath);
    ScopedLibrary hpLib(hpImporterPath);
    ScopedLibrary exporterLib(exporterPath);

    auto normalFn = normalLib.resolve<ImportEntryFn>("xImportEntry");
    auto hpFn = hpLib.resolve<ImportEntryFn>("xImportEntry");
    auto exportFn = exporterLib.resolve<ExportEntryFn>("xSDKExport");

    result.normalImporter.loaded = normalLib.isLoaded() && normalFn != nullptr;
    result.hpImporter.loaded = hpLib.isLoaded() && hpFn != nullptr;
    result.exporterLoaded = exporterLib.isLoaded() && exportFn != nullptr;

    if (!result.normalImporter.loaded || !result.hpImporter.loaded || !result.exporterLoaded) {
        result.errorMessage = "Failed to load Dual-PRM or Exporter plugin binary.";
        return result;
    }

    return validateDualDirectEntryPoints(normalFn, hpFn, exportFn);
}

DeploymentValidationResult DeploymentValidator::validateDirectEntryPoints(
    ImportEntryFn importFn,
    ExportEntryFn exportFn) const {
    DeploymentValidationResult result{};
    if (!importFn || !exportFn) {
        result.errorMessage = "Null entry point function pointer supplied.";
        return result;
    }

    result.importerLoaded = true;
    result.exporterLoaded = true;
    MockPremiereHost host;
    result.importerHandshakeOk = verifyImporterHandshake(
        importFn, host, result.importerPriority, result.avoidAudioConform);
    result.exporterHandshakeOk = verifyExporterHandshake(
        exportFn, host, result.exporterContainerCount);
    return result;
}

DeploymentValidationResult DeploymentValidator::validateDualDirectEntryPoints(
    ImportEntryFn normalImportFn,
    ImportEntryFn hpImportFn,
    ExportEntryFn exportFn,
    const std::string& normalPiplName,
    const std::string& hpPiplName) const {
    DeploymentValidationResult result{};
    result.dualPrmMode = true;
    if (!normalImportFn || !hpImportFn || !exportFn) {
        result.errorMessage = "Null Dual-PRM or Exporter entry point pointer supplied.";
        return result;
    }

    MockPremiereHost host;
    result.normalImporter = evaluateImporterVariant(
        normalImportFn, host, ImporterPriorityTier::Normal, normalPiplName);
    result.hpImporter = evaluateImporterVariant(
        hpImportFn, host, ImporterPriorityTier::High, hpPiplName);

    result.importerLoaded = result.normalImporter.loaded && result.hpImporter.loaded;
    result.importerHandshakeOk = result.normalImporter.handshakeOk && result.hpImporter.handshakeOk;
    result.importerPriority = result.normalImporter.priority;
    result.avoidAudioConform = result.normalImporter.avoidAudioConform && result.hpImporter.avoidAudioConform;

    result.exporterLoaded = true;
    result.exporterHandshakeOk = verifyExporterHandshake(
        exportFn, host, result.exporterContainerCount);
    return result;
}

bool DeploymentValidator::verifyImporterHandshake(
    ImportEntryFn importFn,
    MockPremiereHost& host,
    csSDK_int32& outPriority,
    bool& outAvoidConform) {
    if (!importFn) {
        return false;
    }
    openmedia::importer::ImImportInfoRecExt initRec{};
    if (importFn(openmedia::importer::kSelInit, &host.importerStdParms, &initRec, nullptr) != openmedia::importer::kImNoErr) {
        return false;
    }
    outPriority = initRec.priority;
    outAvoidConform = (initRec.avoidAudioConform != 0);

    for (std::intptr_t idx = 0; idx < 7; ++idx) {
        openmedia::importer::ImIndFormatRecExt fmtRec{};
        const csSDK_int32 rc = importFn(
            openmedia::importer::kSelGetIndFormat,
            &host.importerStdParms,
            reinterpret_cast<void*>(idx),
            &fmtRec);
        if (rc != openmedia::importer::kImNoErr) {
            return false;
        }
    }
    if (importFn(openmedia::importer::kSelQuietFile, &host.importerStdParms, nullptr, nullptr) != openmedia::importer::kImNoErr) {
        return false;
    }
    return outPriority >= 100 && outAvoidConform;
}

bool DeploymentValidator::verifyExporterHandshake(
    ExportEntryFn exportFn,
    MockPremiereHost& host,
    int& outContainerCount) {
    if (!exportFn) {
        return false;
    }
    outContainerCount = 0;
    for (csSDK_int32 idx = 0; idx < 4; ++idx) {
        openmedia::exporter::ExStartupInfoRecExt startupRec{};
        startupRec.exporterIndex = idx;
        const csSDK_int32 rc = exportFn(exSelStartup, &host.exporterStdParms, &startupRec, nullptr);
        if (rc == openmedia::exporter::kExIterateExporter) {
            ++outContainerCount;
            continue;
        }
        if (rc == openmedia::exporter::kExIterateExporterDone) {
            if (!startupRec.fileExtension.empty()) {
                ++outContainerCount;
            }
            break;
        }
        break;
    }
    if (exportFn(exSelBeginInstance, &host.exporterStdParms, nullptr, nullptr) != openmedia::exporter::kExNoErr) {
        return false;
    }
    prBool isStill = 1;
    if (exportFn(10, &host.exporterStdParms, &isStill, nullptr) != openmedia::exporter::kExNoErr) {
        return false;
    }
    return exportFn(exSelEndInstance, &host.exporterStdParms, nullptr, nullptr) == openmedia::exporter::kExNoErr;
}

} // namespace openmedia::integration
