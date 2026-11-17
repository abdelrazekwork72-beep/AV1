#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>

namespace {
void preloadCompanionDllsFromModuleDir(HINSTANCE hinstDLL) noexcept {
    wchar_t modulePath[MAX_PATH] = {};
    const DWORD len = GetModuleFileNameW(hinstDLL, modulePath, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return;
    }
    std::wstring dir(modulePath, len);
    const size_t slash = dir.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return;
    }
    dir.resize(slash + 1);

    static constexpr const wchar_t* kDeps[] = {
        L"avutil-60.dll",
        L"swresample-6.dll",
        L"swscale-9.dll",
        L"avcodec-62.dll",
        L"avformat-62.dll",
    };
    for (const wchar_t* dll : kDeps) {
        const std::wstring fullPath = dir + dll;
        if (GetFileAttributesW(fullPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            LoadLibraryExW(fullPath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        }
    }
}
} // namespace

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID /*lpvReserved*/) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        preloadCompanionDllsFromModuleDir(hinstDLL);
    }
    return TRUE;
}
#endif
