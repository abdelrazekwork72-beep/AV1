#include "desktop/desktop_main_window.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>
#endif

int main(int argc, char** argv) {
    auto opts = openmedia::desktop::DesktopMainWindow::parseCommandLine(argc, argv);
    openmedia::desktop::DesktopMainWindow window(opts);
    window.initialize();
    return window.run();
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    int argc = 0;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::vector<std::string> argsStorage;
    std::vector<char*> argvPtrs;
    if (wargv) {
        argsStorage.reserve(static_cast<size_t>(argc));
        argvPtrs.reserve(static_cast<size_t>(argc));
        for (int i = 0; i < argc; ++i) {
            const int needed = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, nullptr, 0, nullptr, nullptr);
            std::string s(needed > 0 ? static_cast<size_t>(needed - 1) : 0, '\0');
            if (needed > 1) {
                WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, &s[0], needed, nullptr, nullptr);
            }
            argsStorage.push_back(std::move(s));
            argvPtrs.push_back(argsStorage.back().data());
        }
        LocalFree(wargv);
    }
    return main(argc, argvPtrs.data());
}
#endif
