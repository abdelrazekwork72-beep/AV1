#include "desktop/desktop_main_window.hpp"

#include <cstdlib>
#include <filesystem>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#endif

namespace openmedia::desktop {

namespace {

std::string resolveConfigDir() {
#ifdef _WIN32
    if (const char* appData = std::getenv("APPDATA"); appData && *appData) {
        return std::string(appData) + "\\OpenMedia";
    }
    return "C:\\Program Files\\Adobe\\Common\\Plug-ins\\7.0\\MediaCore";
#else
    if (const char* home = std::getenv("HOME"); home && *home) {
        return std::string(home) + "/.config/openmedia";
    }
    return "/tmp/openmedia";
#endif
}

#ifdef _WIN32
std::wstring toWide(const std::string& utf8) {
    if (utf8.empty()) return {};
    const int needed = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
    if (needed <= 0) return {};
    std::wstring out(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), &out[0], needed);
    return out;
}

std::string toUtf8(const std::wstring& wide) {
    if (wide.empty()) return {};
    const int needed = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return {};
    std::string out(static_cast<size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), &out[0], needed, nullptr, nullptr);
    return out;
}

constexpr COLORREF kBgColor = RGB(18, 18, 22);
constexpr COLORREF kSurfaceColor = RGB(30, 30, 36);
constexpr COLORREF kTextColor = RGB(241, 245, 249);

constexpr int kIdTabControl = 1001;
constexpr int kIdLangToggle = 1002;
constexpr int kIdSaveConfig = 1003;
constexpr int kIdResetDefaults = 1004;
constexpr int kIdRefreshGpu = 1005;
constexpr int kIdClearThumbCache = 1006;
constexpr int kIdClearMediaCache = 1007;
constexpr int kIdExportDiag = 1008;
constexpr int kIdCopySuffix = 1009;
constexpr int kIdBodyText = 1010;
constexpr int kIdStatusBar = 1011;
constexpr int kIdPolicyCombo = 1012;
constexpr int kIdGpuMinW = 1013;
constexpr int kIdGpuMinH = 1014;
constexpr int kIdMaxGpuStreams = 1015;

struct Win32UiState {
    DesktopMainWindow* owner = nullptr;
    HWND hwnd = nullptr;
    HWND tab = nullptr;
    HWND langBtn = nullptr;
    HWND policyCombo = nullptr;
    HWND gpuMinWEdit = nullptr;
    HWND gpuMinHEdit = nullptr;
    HWND maxGpuEdit = nullptr;
    HWND saveBtn = nullptr;
    HWND resetBtn = nullptr;
    HWND refreshGpuBtn = nullptr;
    HWND clearThumbBtn = nullptr;
    HWND clearMediaBtn = nullptr;
    HWND exportDiagBtn = nullptr;
    HWND copySuffixBtn = nullptr;
    HWND bodyEdit = nullptr;
    HWND statusLabel = nullptr;
    HBRUSH bgBrush = nullptr;
    HBRUSH surfaceBrush = nullptr;
    HFONT uiFont = nullptr;
};

void syncTabVisibility(Win32UiState* st) {
    if (!st || !st->owner) return;
    const DesktopTab tab = st->owner->activeTab();
    const bool isSettings = (tab == DesktopTab::Settings);
    const bool isGpu = (tab == DesktopTab::GpuStatus);
    const bool isSuffix = (tab == DesktopTab::SuffixModifiers);
    const bool isCache = (tab == DesktopTab::CacheAndGuide);

    ShowWindow(st->policyCombo, isSettings ? SW_SHOW : SW_HIDE);
    ShowWindow(st->gpuMinWEdit, isSettings ? SW_SHOW : SW_HIDE);
    ShowWindow(st->gpuMinHEdit, isSettings ? SW_SHOW : SW_HIDE);
    ShowWindow(st->maxGpuEdit, isSettings ? SW_SHOW : SW_HIDE);
    ShowWindow(st->saveBtn, isSettings ? SW_SHOW : SW_HIDE);
    ShowWindow(st->resetBtn, isSettings ? SW_SHOW : SW_HIDE);

    ShowWindow(st->refreshGpuBtn, isGpu ? SW_SHOW : SW_HIDE);
    ShowWindow(st->copySuffixBtn, isSuffix ? SW_SHOW : SW_HIDE);
    ShowWindow(st->clearThumbBtn, isCache ? SW_SHOW : SW_HIDE);
    ShowWindow(st->clearMediaBtn, isCache ? SW_SHOW : SW_HIDE);
    ShowWindow(st->exportDiagBtn, isCache ? SW_SHOW : SW_HIDE);
    ShowWindow(st->bodyEdit, (isGpu || isSuffix || isCache) ? SW_SHOW : SW_HIDE);
}

void refreshLocalizedUi(Win32UiState* st) {
    if (!st || !st->owner) return;
    const auto& i18n = st->owner->i18n();
    const bool rtl = st->owner->isRtlLayout();

    LONG_PTR exStyle = GetWindowLongPtrW(st->hwnd, GWL_EXSTYLE);
    if (rtl) {
        exStyle |= WS_EX_LAYOUTRTL;
    } else {
        exStyle &= ~static_cast<LONG_PTR>(WS_EX_LAYOUTRTL);
    }
    SetWindowLongPtrW(st->hwnd, GWL_EXSTYLE, exStyle);

    const std::string titleKey = st->owner->isPopupMode() ? "app.popup_title" : "app.title";
    SetWindowTextW(st->hwnd, toWide(i18n.tr(titleKey)).c_str());
    SetWindowTextW(st->langBtn, toWide(i18n.tr("lang.switch")).c_str());
    SetWindowTextW(st->saveBtn, toWide(i18n.tr("btn.save")).c_str());
    SetWindowTextW(st->resetBtn, toWide(i18n.tr("btn.reset")).c_str());
    SetWindowTextW(st->refreshGpuBtn, toWide(i18n.tr("btn.refresh")).c_str());
    SetWindowTextW(st->clearThumbBtn, toWide(i18n.tr("btn.clear_thumb_cache")).c_str());
    SetWindowTextW(st->clearMediaBtn, toWide(i18n.tr("btn.clear_media_cache")).c_str());
    SetWindowTextW(st->exportDiagBtn, toWide(i18n.tr("btn.export_diag")).c_str());
    SetWindowTextW(st->copySuffixBtn, toWide(i18n.tr("btn.copy_filename")).c_str());

    const char* tabKeys[4] = {"tab.settings", "tab.gpu", "tab.suffix", "tab.cache_guide"};
    for (int i = 0; i < 4; ++i) {
        std::wstring label = toWide(i18n.tr(tabKeys[i]));
        TCITEMW item{};
        item.mask = TCIF_TEXT;
        item.pszText = label.data();
        SendMessageW(st->tab, TCM_SETITEMW, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(&item));
    }

    const DesktopTab tab = st->owner->activeTab();
    if (tab == DesktopTab::GpuStatus) {
        auto rows = st->owner->refreshTelemetryRows();
        std::ostringstream oss;
        if (rows.empty()) {
            oss << i18n.tr("gpu.empty");
        } else {
            for (const auto& r : rows) {
                oss << r.fileName << " | " << r.codec << " | " << r.resolution
                    << " | " << r.backendBadge << " | " << r.statusSummary << "\r\n";
            }
        }
        SetWindowTextW(st->bodyEdit, toWide(oss.str()).c_str());
    } else if (tab == DesktopTab::SuffixModifiers) {
        SuffixModifierOptions sampleOpts;
        sampleOpts.width = 1920;
        sampleOpts.height = 1080;
        sampleOpts.colorRange = "full";
        sampleOpts.accurateDuration = true;
        std::string preview = SuffixModifierBuilder::buildFilename("C:\\Media\\clip.mkv", sampleOpts);
        SetWindowTextW(st->bodyEdit, toWide(preview).c_str());
    } else if (tab == DesktopTab::CacheAndGuide) {
        std::ostringstream oss;
        auto health = st->owner->refreshPluginHealth();
        for (const auto& h : health) {
            oss << (h.present ? "[OK] " : "[MISSING] ") << h.componentName << " -> " << h.expectedPath << "\r\n";
        }
        oss << "\r\n" << st->owner->buildUserGuideText();
        SetWindowTextW(st->bodyEdit, toWide(oss.str()).c_str());
    }

    syncTabVisibility(st);
    InvalidateRect(st->hwnd, nullptr, TRUE);
}

LRESULT CALLBACK DesktopWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* st = reinterpret_cast<Win32UiState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_CREATE: {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            st = reinterpret_cast<Win32UiState*>(cs->lpCreateParams);
            st->hwnd = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
            st->bgBrush = CreateSolidBrush(kBgColor);
            st->surfaceBrush = CreateSolidBrush(kSurfaceColor);
            st->uiFont = CreateFontW(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

            RECT rc{};
            GetClientRect(hwnd, &rc);
            const int w = rc.right - rc.left;
            const int h = rc.bottom - rc.top;

            st->langBtn = CreateWindowExW(0, L"BUTTON", L"AR / EN",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, w - 130, 10, 114, 28,
                hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdLangToggle)), nullptr, nullptr);

            st->tab = CreateWindowExW(0, WC_TABCONTROLW, L"",
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 16, 46, w - 32, h - 96,
                hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdTabControl)), nullptr, nullptr);

            for (int i = 0; i < 4; ++i) {
                TCITEMW item{};
                item.mask = TCIF_TEXT;
                wchar_t buf[] = L"Tab";
                item.pszText = buf;
                SendMessageW(st->tab, TCM_INSERTITEMW, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(&item));
            }

            st->policyCombo = CreateWindowExW(0, L"COMBOBOX", L"",
                WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, 32, 92, w - 64, 140,
                hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdPolicyCombo)), nullptr, nullptr);
            SendMessageW(st->policyCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"force_override_all_adobe"));
            SendMessageW(st->policyCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"new_formats_codecs_and_fallback"));
            SendMessageW(st->policyCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"new_formats_and_fallback_only"));
            SendMessageW(st->policyCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"disabled"));
            SendMessageW(st->policyCombo, CB_SETCURSEL, 1, 0);

            const auto& cfg = st->owner->config();
            st->gpuMinWEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
                toWide(std::to_string(cfg.maxFrameCacheSizeMB)).c_str(),
                WS_CHILD | ES_NUMBER, 32, 134, 140, 28,
                hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdGpuMinW)), nullptr, nullptr);
            st->gpuMinHEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
                toWide(std::to_string(cfg.maxPercentUsageAvailableMemory)).c_str(),
                WS_CHILD | ES_NUMBER, 188, 134, 140, 28,
                hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdGpuMinH)), nullptr, nullptr);
            st->maxGpuEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
                toWide(std::to_string(cfg.maxLogFileSizeMB)).c_str(),
                WS_CHILD | ES_NUMBER, 344, 134, 140, 28,
                hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdMaxGpuStreams)), nullptr, nullptr);

            st->saveBtn = CreateWindowExW(0, L"BUTTON", L"Save",
                WS_CHILD | BS_PUSHBUTTON, 32, 180, 150, 32,
                hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdSaveConfig)), nullptr, nullptr);
            st->resetBtn = CreateWindowExW(0, L"BUTTON", L"Reset",
                WS_CHILD | BS_PUSHBUTTON, 196, 180, 150, 32,
                hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdResetDefaults)), nullptr, nullptr);

            st->refreshGpuBtn = CreateWindowExW(0, L"BUTTON", L"Refresh",
                WS_CHILD | BS_PUSHBUTTON, 32, 92, 140, 30,
                hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdRefreshGpu)), nullptr, nullptr);
            st->copySuffixBtn = CreateWindowExW(0, L"BUTTON", L"Copy",
                WS_CHILD | BS_PUSHBUTTON, 32, 92, 180, 30,
                hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdCopySuffix)), nullptr, nullptr);
            st->clearThumbBtn = CreateWindowExW(0, L"BUTTON", L"Clear Thumb",
                WS_CHILD | BS_PUSHBUTTON, 32, 92, 160, 30,
                hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdClearThumbCache)), nullptr, nullptr);
            st->clearMediaBtn = CreateWindowExW(0, L"BUTTON", L"Clear Media",
                WS_CHILD | BS_PUSHBUTTON, 204, 92, 160, 30,
                hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdClearMediaCache)), nullptr, nullptr);
            st->exportDiagBtn = CreateWindowExW(0, L"BUTTON", L"Export Diag",
                WS_CHILD | BS_PUSHBUTTON, 376, 92, 160, 30,
                hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdExportDiag)), nullptr, nullptr);

            st->bodyEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL,
                32, 134, w - 64, h - 200,
                hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdBodyText)), nullptr, nullptr);

            st->statusLabel = CreateWindowExW(0, L"STATIC", L"OpenMedia v3.0.0 Ready",
                WS_CHILD | WS_VISIBLE, 16, h - 38, w - 32, 24,
                hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdStatusBar)), nullptr, nullptr);

            HWND controls[] = {
                st->langBtn, st->tab, st->policyCombo, st->gpuMinWEdit, st->gpuMinHEdit,
                st->maxGpuEdit, st->saveBtn, st->resetBtn, st->refreshGpuBtn,
                st->clearThumbBtn, st->clearMediaBtn, st->exportDiagBtn,
                st->copySuffixBtn, st->bodyEdit, st->statusLabel
            };
            for (HWND c : controls) {
                SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(st->uiFont), TRUE);
            }

            refreshLocalizedUi(st);
            return 0;
        }
        case WM_NOTIFY: {
            auto* nmhdr = reinterpret_cast<NMHDR*>(lParam);
            if (st && nmhdr && nmhdr->idFrom == kIdTabControl && nmhdr->code == TCN_SELCHANGE) {
                const int sel = TabCtrl_GetCurSel(st->tab);
                if (sel >= 0 && sel <= 3) {
                    st->owner->selectTab(static_cast<DesktopTab>(sel));
                    refreshLocalizedUi(st);
                }
            }
            return 0;
        }
        case WM_COMMAND: {
            if (!st || !st->owner) break;
            const int id = LOWORD(wParam);
            if (id == kIdLangToggle) {
                const UiLanguage next = (st->owner->language() == UiLanguage::English)
                    ? UiLanguage::Arabic : UiLanguage::English;
                st->owner->setLanguage(next);
                refreshLocalizedUi(st);
            } else if (id == kIdSaveConfig) {
                wchar_t buf[64]{};
                GetWindowTextW(st->gpuMinWEdit, buf, 64);
                st->owner->config().maxFrameCacheSizeMB = static_cast<uint32_t>((std::max)(0, std::atoi(toUtf8(buf).c_str())));
                GetWindowTextW(st->gpuMinHEdit, buf, 64);
                st->owner->config().maxPercentUsageAvailableMemory = static_cast<uint32_t>((std::max)(0, std::atoi(toUtf8(buf).c_str())));
                GetWindowTextW(st->maxGpuEdit, buf, 64);
                st->owner->config().maxLogFileSizeMB = (std::max)(1, std::atoi(toUtf8(buf).c_str()));
                const LRESULT pol = SendMessageW(st->policyCombo, CB_GETCURSEL, 0, 0);
                static const char* kPolicies[4] = {
                    "force_override_all_adobe", "new_formats_codecs_and_fallback",
                    "new_formats_and_fallback_only", "disabled"
                };
                if (pol >= 0 && pol < 4) {
                    st->owner->config().importPolicy = kPolicies[pol];
                }
                const bool ok = st->owner->saveCurrentConfig();
                SetWindowTextW(st->statusLabel, toWide(ok ? st->owner->i18n().tr("status.saved")
                                                          : st->owner->i18n().tr("status.save_error")).c_str());
            } else if (id == kIdResetDefaults) {
                st->owner->resetToFactoryDefaults();
                SetWindowTextW(st->gpuMinWEdit, toWide(std::to_string(st->owner->config().maxFrameCacheSizeMB)).c_str());
                SetWindowTextW(st->gpuMinHEdit, toWide(std::to_string(st->owner->config().maxPercentUsageAvailableMemory)).c_str());
                SetWindowTextW(st->maxGpuEdit, toWide(std::to_string(st->owner->config().maxLogFileSizeMB)).c_str());
                SendMessageW(st->policyCombo, CB_SETCURSEL, 1, 0);
                refreshLocalizedUi(st);
            } else if (id == kIdRefreshGpu) {
                refreshLocalizedUi(st);
            } else if (id == kIdClearThumbCache) {
                auto res = CacheAndLogManager::clearThumbnailCache(CacheAndLogManager::defaultThumbnailCachePath());
                SetWindowTextW(st->statusLabel, toWide(res.statusMessage).c_str());
            } else if (id == kIdClearMediaCache) {
                auto res = CacheAndLogManager::clearPremiereMediaCache(CacheAndLogManager::defaultPremiereMediaCacheDir());
                SetWindowTextW(st->statusLabel, toWide(res.statusMessage).c_str());
            } else if (id == kIdExportDiag) {
                const std::string outZip = resolveConfigDir() + "\\OpenMedia_Diagnostics.txt";
                auto res = CacheAndLogManager::exportDiagnosticBundle(
                    DesktopMainWindow::defaultConfigPath(),
                    DesktopMainWindow::defaultTelemetryPath(),
                    outZip);
                SetWindowTextW(st->statusLabel, toWide(res.statusMessage).c_str());
            }
            return 0;
        }
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORBTN: {
            if (st && st->surfaceBrush) {
                HDC hdc = reinterpret_cast<HDC>(wParam);
                SetTextColor(hdc, kTextColor);
                SetBkColor(hdc, kSurfaceColor);
                return reinterpret_cast<LRESULT>(st->surfaceBrush);
            }
            break;
        }
        case WM_ERASEBKGND: {
            if (st && st->bgBrush) {
                RECT rc{};
                GetClientRect(hwnd, &rc);
                FillRect(reinterpret_cast<HDC>(wParam), &rc, st->bgBrush);
                return 1;
            }
            break;
        }
        case WM_DESTROY: {
            if (st) {
                if (st->bgBrush) DeleteObject(st->bgBrush);
                if (st->surfaceBrush) DeleteObject(st->surfaceBrush);
                if (st->uiFont) DeleteObject(st->uiFont);
            }
            PostQuitMessage(0);
            return 0;
        }
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
#endif

} // namespace

DesktopMainWindow::DesktopMainWindow(DesktopLaunchOptions opts)
    : opts_(std::move(opts)),
      i18n_(opts_.langOverrideSet ? opts_.langOverride : UiLanguage::English),
      config_(ConfigTomlEditor::factoryDefaults()) {
    if (opts_.configPath.empty()) {
        opts_.configPath = defaultConfigPath();
    }
    if (opts_.telemetryPath.empty()) {
        opts_.telemetryPath = defaultTelemetryPath();
    }
}

DesktopLaunchOptions DesktopMainWindow::parseCommandLine(int argc, char** argv) {
    DesktopLaunchOptions opts;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i] ? argv[i] : "";
        if (arg == "--popup") {
            opts.popupMode = true;
        } else if (arg == "--headless-self-test") {
            opts.headlessSelfTest = true;
        } else if (arg == "--lang" && i + 1 < argc) {
            const std::string val = argv[++i] ? argv[i] : "";
            opts.langOverrideSet = true;
            opts.langOverride = (val == "ar" || val == "AR") ? UiLanguage::Arabic : UiLanguage::English;
        } else if (arg == "--config" && i + 1 < argc) {
            opts.configPath = argv[++i] ? argv[i] : "";
        } else if (arg == "--telemetry" && i + 1 < argc) {
            opts.telemetryPath = argv[++i] ? argv[i] : "";
        }
    }
    return opts;
}

std::string DesktopMainWindow::defaultConfigPath() {
#ifdef _WIN32
    return resolveConfigDir() + "\\Config.toml";
#else
    return resolveConfigDir() + "/Config.toml";
#endif
}

std::string DesktopMainWindow::defaultTelemetryPath() {
#ifdef _WIN32
    return resolveConfigDir() + "\\telemetry_snapshot.json";
#else
    return resolveConfigDir() + "/telemetry_snapshot.json";
#endif
}

void DesktopMainWindow::initialize() {
    if (std::filesystem::exists(opts_.configPath)) {
        config_ = ConfigTomlEditor::loadFromFile(opts_.configPath);
    } else {
        config_ = ConfigTomlEditor::factoryDefaults();
    }
    if (!opts_.langOverrideSet) {
        i18n_.setLanguage(config_.uiLanguage == "ar" ? UiLanguage::Arabic : UiLanguage::English);
    } else {
        config_.uiLanguage = i18n_.languageCode();
    }
    initialized_ = true;
}

int DesktopMainWindow::run() {
    if (!initialized_) {
        initialize();
    }
    if (opts_.headlessSelfTest) {
        return runHeadlessSelfTest();
    }

#ifdef _WIN32
    INITCOMMONCONTROLSEX icc{sizeof(INITCOMMONCONTROLSEX), ICC_TAB_CLASSES | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);

    const HINSTANCE hInst = GetModuleHandleW(nullptr);
    const wchar_t* kClassName = L"OpenMediaDesktopMainWindow";

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DesktopWndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);

    Win32UiState state;
    state.owner = this;

    const int width = opts_.popupMode ? 760 : 940;
    const int height = opts_.popupMode ? 540 : 640;
    const DWORD exStyle = isRtlLayout() ? WS_EX_LAYOUTRTL : 0;
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

    const int screenW = GetSystemMetrics(SM_CXSCREEN);
    const int screenH = GetSystemMetrics(SM_CYSCREEN);
    const int posX = (screenW - width) / 2;
    const int posY = (screenH - height) / 2;

    const std::string titleKey = opts_.popupMode ? "app.popup_title" : "app.title";
    HWND hwnd = CreateWindowExW(
        exStyle, kClassName, toWide(i18n_.tr(titleKey)).c_str(),
        style, posX, posY, width, height,
        nullptr, nullptr, hInst, &state);
    if (!hwnd) {
        return 1;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
#else
    return 0;
#endif
}

int DesktopMainWindow::runHeadlessSelfTest() {
    if (!initialized_) {
        initialize();
    }
    if (i18n_.keyCount() < 20) return 2;
    const UiLanguage originalLang = i18n_.language();
    setLanguage(UiLanguage::Arabic);
    if (!isRtlLayout()) return 3;
    setLanguage(UiLanguage::English);
    if (isRtlLayout()) return 4;
    setLanguage(originalLang);

    const std::string toml = ConfigTomlEditor::serializeToToml(config_);
    if (toml.find("gpuDecodeMode") == std::string::npos) return 5;

    SuffixModifierOptions opts;
    opts.width = 1920;
    opts.height = 1080;
    const std::string mod = SuffixModifierBuilder::buildFilename("clip.mkv", opts);
    if (mod.find("!w=1920!h=1080") == std::string::npos) return 6;

    return 0;
}

void DesktopMainWindow::setLanguage(UiLanguage lang) {
    i18n_.setLanguage(lang);
    config_.uiLanguage = i18n_.languageCode();
}

UiLanguage DesktopMainWindow::language() const noexcept {
    return i18n_.language();
}

bool DesktopMainWindow::isRtlLayout() const noexcept {
    return i18n_.isRtl();
}

void DesktopMainWindow::selectTab(DesktopTab tab) noexcept {
    activeTab_ = tab;
}

DesktopTab DesktopMainWindow::activeTab() const noexcept {
    return activeTab_;
}

bool DesktopMainWindow::isPopupMode() const noexcept {
    return opts_.popupMode;
}

const I18nEngine& DesktopMainWindow::i18n() const noexcept {
    return i18n_;
}

importer::OpenMediaConfig& DesktopMainWindow::config() noexcept {
    return config_;
}

const importer::OpenMediaConfig& DesktopMainWindow::config() const noexcept {
    return config_;
}

bool DesktopMainWindow::saveCurrentConfig() {
    return ConfigTomlEditor::saveToFile(opts_.configPath, config_);
}

void DesktopMainWindow::resetToFactoryDefaults() {
    const std::string currentLang = i18n_.languageCode();
    config_ = ConfigTomlEditor::factoryDefaults();
    config_.uiLanguage = currentLang;
}

std::vector<GpuStreamDisplayRow> DesktopMainWindow::refreshTelemetryRows() {
    auto telemetry = GpuStatusMonitor::loadFromFile(opts_.telemetryPath);
    return GpuStatusMonitor::formatRows(telemetry, i18n_.language());
}

std::vector<PluginHealthItem> DesktopMainWindow::refreshPluginHealth(const std::string& mediaCoreDir) const {
    const std::string dir = mediaCoreDir.empty() ? PluginHealthAndGuide::defaultMediaCoreDir() : mediaCoreDir;
    return PluginHealthAndGuide::inspectInstallation(dir);
}

std::string DesktopMainWindow::buildUserGuideText() const {
    return PluginHealthAndGuide::generateBilingualUserGuide(i18n_.language());
}

} // namespace openmedia::desktop
