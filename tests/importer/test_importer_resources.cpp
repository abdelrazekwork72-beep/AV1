/// @file test_importer_resources.cpp
/// @brief Standalone C++17 unit test (TC-2.1.2) for IMPT & PiPL importer resources.

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

inline constexpr std::uint32_t kOpenMediaImporterFourCC = 0x4F4D4950U;
inline constexpr std::uint32_t kAfterEffectsReservedFourCC = 0x4D4F6F76U;

std::filesystem::path resolveRepoPath(const std::string& relativePath) {
    const std::vector<std::filesystem::path> prefixes = {
        ".",
        "..",
        "../.."
    };
    for (const auto& prefix : prefixes) {
        const auto candidate = prefix / relativePath;
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    return std::filesystem::path(relativePath);
}

std::string readFileToString(const std::string& relativePath) {
    const auto fullPath = resolveRepoPath(relativePath);
    std::ifstream ifs(fullPath, std::ios::in | std::ios::binary);
    assert(ifs.is_open() && "Failed to open repository file");
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

void assertContains(const std::string& haystack, const std::string& needle, const char* context) {
    if (haystack.find(needle) == std::string::npos) {
        std::cerr << "[FAIL] Expected token '" << needle << "' not found in " << context << '\n';
        assert(false && "Missing required resource token");
    }
}

void assertNotContains(const std::string& haystack, const std::string& needle, const char* context) {
    if (haystack.find(needle) != std::string::npos) {
        std::cerr << "[FAIL] Forbidden token '" << needle << "' found in " << context << '\n';
        assert(false && "Forbidden resource token present");
    }
}

void testImporterFourCCConstant() {
    static_assert(kOpenMediaImporterFourCC == 0x4F4D4950U,
                  "kOpenMediaImporterFourCC must be 0x4F4D4950U ('OMIP')");
    static_assert(kOpenMediaImporterFourCC != kAfterEffectsReservedFourCC,
                  "kOpenMediaImporterFourCC must not collide with 0x4D4F6F76U ('MOov')");

    assert(kOpenMediaImporterFourCC == 0x4F4D4950U);
    assert(kOpenMediaImporterFourCC != kAfterEffectsReservedFourCC);

    const std::string typesHeader = readFileToString("include/importer/importer_types.hpp");
    assertContains(typesHeader, "kOpenMediaImporterFourCC", "include/importer/importer_types.hpp");
    assertContains(typesHeader, "0x4F4D4950U", "include/importer/importer_types.hpp");
}

void testWindowsResourceScript() {
    const std::string rc = readFileToString("resources/importer_resources.rc");
    assert(!rc.empty());

    assertContains(rc, "IMPT", "resources/importer_resources.rc");
    assertContains(rc, "PiPL", "resources/importer_resources.rc");
    assertContains(rc, "0x4F4D4950", "resources/importer_resources.rc");
    assertContains(rc, "OpenMedia Importer", "resources/importer_resources.rc");
    assertContains(rc, "1.0.0.0", "resources/importer_resources.rc");
    assertContains(rc, "xImportEntry", "resources/importer_resources.rc");

    assertNotContains(rc, "0x4D4F6F76", "resources/importer_resources.rc");
    assertNotContains(rc, "4D4F6F76", "resources/importer_resources.rc");
}

void testMacOSRezPiPLResource() {
    const std::string rez = readFileToString("resources/importer_pipl.r");
    assert(!rez.empty());

    assertContains(rez, "PiPL", "resources/importer_pipl.r");
    assertContains(rez, "IMPT", "resources/importer_pipl.r");
    assertContains(rez, "OMIP", "resources/importer_pipl.r");
    assertContains(rez, "0x4F4D4950", "resources/importer_pipl.r");
    assertContains(rez, "xImportEntry", "resources/importer_pipl.r");

    assertNotContains(rez, "0x4D4F6F76", "resources/importer_pipl.r");
    assertNotContains(rez, "4D4F6F76", "resources/importer_pipl.r");
}

void testMacOSInfoPlistTemplate() {
    const std::string plist = readFileToString("resources/Info_Importer.plist.in");
    assert(!plist.empty());

    assertContains(plist, "CFBundleIdentifier", "resources/Info_Importer.plist.in");
    assertContains(plist, "com.openmedia.premiere.OpenMedia_Importer", "resources/Info_Importer.plist.in");
    assertContains(plist, "CFBundlePackageType", "resources/Info_Importer.plist.in");
    assertContains(plist, "BNDL", "resources/Info_Importer.plist.in");
}

}  // namespace

int main() {
    testImporterFourCCConstant();
    testWindowsResourceScript();
    testMacOSRezPiPLResource();
    testMacOSInfoPlistTemplate();

    std::cout << "[PASS] TC-2.1.2: All IMPT & PiPL importer resource checks passed.\n";
    return 0;
}
