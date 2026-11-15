#include "desktop/plugin_health_guide.hpp"

#include <array>
#include <filesystem>

namespace openmedia::desktop {

std::string PluginHealthAndGuide::defaultMediaCoreDir() {
#ifdef _WIN32
    return "C:\\Program Files\\Adobe\\Common\\Plug-ins\\7.0\\MediaCore";
#else
    return "/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore";
#endif
}

std::vector<PluginHealthItem> PluginHealthAndGuide::inspectInstallation(const std::string& mediaCoreDir) {
    static constexpr std::array<const char*, 5> kComponents = {
        "OpenMedia_Importer.prm",
        "OpenMedia_ImporterHP.prm",
        "OpenMedia_Exporter.prm",
        "OpenMediaCore.dll",
        "Config.toml"
    };
    std::vector<PluginHealthItem> items;
    const std::filesystem::path base(mediaCoreDir);
    for (const char* name : kComponents) {
        PluginHealthItem item;
        item.componentName = name;
        const std::filesystem::path full = base / name;
        item.expectedPath = full.string();
        std::error_code ec;
        if (std::filesystem::exists(full, ec) && std::filesystem::is_regular_file(full, ec)) {
            item.present = true;
            item.sizeBytes = std::filesystem::file_size(full, ec);
        }
        items.push_back(std::move(item));
    }
    return items;
}

std::string PluginHealthAndGuide::generateBilingualUserGuide(UiLanguage lang) {
    if (lang == UiLanguage::Arabic) {
        return "دليل استخدام OpenMedia v3.0:\n"
               "1. تجاوز مستوردات Adobe: يمكنك فرض استخدام الإضافة عبر إضافة .influx أو .openmedia لاسم الملف.\n"
               "2. لواحق التعديل الفوري (!options): أضف مثلاً !w=1920!cr=full!rft=1.influx قبل الامتداد.\n"
               "3. تسريع GPU: يعمل تلقائياً للملفات >= 2560x1600 بحد أقصى 4 مسارات متزامنة عبر NVDEC/D3D11VA.\n"
               "4. حل مشاكل الصوت/الكاش: استخدم زر تنظيف كاش Premiere لإعادة بناء الموجات الصوتية.";
    }
    return "OpenMedia v3.0 User Guide:\n"
           "1. Override Adobe: Append .influx or .openmedia to any filename to force OpenMedia decoding.\n"
           "2. Filename Suffix Modifiers (!options): Append e.g. !w=1920!cr=full!rft=1.influx.\n"
           "3. GPU Decoding: Recommended mode accelerates >= 2560x1600 streams (max 4 concurrent GPU slots).\n"
           "4. Troubleshooting: Use Clear Premiere Pro Media Cache to force fresh audio conforming.";
}

} // namespace openmedia::desktop
