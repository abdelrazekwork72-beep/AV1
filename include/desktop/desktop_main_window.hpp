#pragma once
/// @file desktop_main_window.hpp
/// @brief Bilingual (EN LTR / AR RTL) Dark Studio Control Center & Host Popup Window.

#include "desktop/desktop_types.hpp"
#include "desktop/i18n_engine.hpp"
#include "desktop/config_toml_editor.hpp"
#include "desktop/gpu_status_monitor.hpp"
#include "desktop/plugin_health_guide.hpp"
#include "desktop/cache_log_manager.hpp"
#include "desktop/suffix_modifier_builder.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace openmedia::desktop {

/// @brief CLI launch options parsed by OpenMediaDesktop.exe.
struct DesktopLaunchOptions {
    bool popupMode = false;          ///< Launched from Premiere Pro / AME (imGetPrefs8)
    bool headlessSelfTest = false;   ///< --headless-self-test CI verification mode
    bool langOverrideSet = false;    ///< Explicit --lang flag provided
    UiLanguage langOverride = UiLanguage::English;
    std::string configPath;          ///< Optional override for Config.toml path
    std::string telemetryPath;       ///< Optional override for telemetry JSON path
};

/// @brief Active tab indices in the 4-tab Control Center.
enum class DesktopTab : int32_t {
    Settings = 0,
    GpuStatus = 1,
    SuffixModifiers = 2,
    CacheAndGuide = 3
};

/// @brief Cross-platform state & Win32 Dark Studio host for OpenMediaDesktop.exe.
class DesktopMainWindow {
public:
    explicit DesktopMainWindow(DesktopLaunchOptions opts = {});

    static DesktopLaunchOptions parseCommandLine(int argc, char** argv);
    static std::string defaultConfigPath();
    static std::string defaultTelemetryPath();

    void initialize();
    int run();
    int runHeadlessSelfTest();

    // State & Localization Accessors (testable cross-platform)
    void setLanguage(UiLanguage lang);
    UiLanguage language() const noexcept;
    bool isRtlLayout() const noexcept;
    void selectTab(DesktopTab tab) noexcept;
    DesktopTab activeTab() const noexcept;
    bool isPopupMode() const noexcept;

    const I18nEngine& i18n() const noexcept;
    importer::OpenMediaConfig& config() noexcept;
    const importer::OpenMediaConfig& config() const noexcept;
    bool saveCurrentConfig();
    void resetToFactoryDefaults();

    std::vector<GpuStreamDisplayRow> refreshTelemetryRows();
    std::vector<PluginHealthItem> refreshPluginHealth(const std::string& mediaCoreDir = "") const;
    std::string buildUserGuideText() const;

private:
    DesktopLaunchOptions opts_;
    I18nEngine i18n_;
    importer::OpenMediaConfig config_;
    DesktopTab activeTab_ = DesktopTab::Settings;
    bool initialized_ = false;
};

} // namespace openmedia::desktop
