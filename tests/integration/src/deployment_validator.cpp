/// @file deployment_validator.cpp
/// @brief Implementation of DeploymentValidator for cross-platform MediaCore plugin verification.

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
            handle_ = LoadLibraryExA(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
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

constexpr csSDK_int32 kSelImInit = 0;
constexpr csSDK_int32 kSelImGetIndFormat = 1;
constexpr csSDK_int32 kSelImOpenFile8 = 2;
constexpr csSDK_int32 kSelImGetInfo8 = 3;
constexpr csSDK_int32 kSelImGetSourceVideo = 4;
constexpr csSDK_int32 kSelImImportAudio7 = 5;

constexpr csSDK_int32 kSelExStartup = 0;
constexpr csSDK_int32 kSelExBeginInstance = 1;
constexpr csSDK_int32 kSelExQueryStillSequence = 10;
constexpr csSDK_int32 kSelExEndInstance = 2;

} // namespace

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

bool DeploymentValidator::verifyImporterHandshake(
    ImportEntryFn importFn,
    MockPremiereHost& host,
    csSDK_int32& outPriority,
    bool& outAvoidConform) {
    if (!importFn) {
        return false;
    }
    std::array<csSDK_int32, 2> initFlags{0, 0};
    if (importFn(kSelImInit, &host.importerStdParms, initFlags.data(), nullptr) != 0) {
        return false;
    }
    outPriority = initFlags[0];
    outAvoidConform = (initFlags[1] != 0);

    const std::array<csSDK_int32, 5> followUpSelectors{
        kSelImGetIndFormat, kSelImOpenFile8, kSelImGetInfo8,
        kSelImGetSourceVideo, kSelImImportAudio7
    };
    for (csSDK_int32 sel : followUpSelectors) {
        if (importFn(sel, &host.importerStdParms, nullptr, nullptr) != 0) {
            return false;
        }
    }
    return outPriority == 100 && outAvoidConform;
}

bool DeploymentValidator::verifyExporterHandshake(
    ExportEntryFn exportFn,
    MockPremiereHost& host,
    int& outContainerCount) {
    if (!exportFn) {
        return false;
    }
    outContainerCount = 0;
    for (int idx = 0; idx < 4; ++idx) {
        const csSDK_int32 rc = exportFn(kSelExStartup, &host.exporterStdParms, &idx, nullptr);
        if (rc == 0) {
            ++outContainerCount;
            continue;
        }
        break;
    }
    if (exportFn(kSelExBeginInstance, &host.exporterStdParms, nullptr, nullptr) != 0) {
        return false;
    }
    if (exportFn(kSelExQueryStillSequence, &host.exporterStdParms, nullptr, nullptr) != 0) {
        return false;
    }
    return exportFn(kSelExEndInstance, &host.exporterStdParms, nullptr, nullptr) == 0;
}

} // namespace openmedia::integration
