# CPackConfig_Windows.cmake — Windows NSIS (.exe) & WiX Installer Configuration (v2.0.0)
# Targets C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore with Administrator ACLs and silent /S support.

set(CPACK_PACKAGE_NAME "OpenMediaPremierePlugin")
set(CPACK_PACKAGE_VENDOR "OpenMedia Project")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Open-Source AV1/VP9/MKV Dual-PRM Importer & Exporter for Adobe Premiere Pro & Media Encoder")
set(CPACK_PACKAGE_VERSION_MAJOR "2")
set(CPACK_PACKAGE_VERSION_MINOR "0")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_PACKAGE_VERSION "2.0.0")
set(CPACK_PACKAGE_FILE_NAME "OpenMedia-PremierePlugin-v2.0.0-Windows-x64-Setup")

set(OPENMEDIA_WIN_MEDIACORE_DIR "C:\\Program Files\\Adobe\\Common\\Plug-ins\\7.0\\MediaCore")
set(CPACK_PACKAGING_INSTALL_PREFIX "${OPENMEDIA_WIN_MEDIACORE_DIR}")

# Packaged binaries & config: Dual-PRM (Normal + HP), Exporter, OpenMediaCore.dll, Config.toml, licenses & README
set(OPENMEDIA_WINDOWS_ARTIFACTS
    "OpenMedia_Importer.prm"
    "OpenMedia_ImporterHP.prm"
    "OpenMedia_Exporter.prm"
    "OpenMediaCore.dll"
    "Config.toml"
    "THIRD_PARTY_LICENSES.md"
    "README.md"
)

# WiX MSI Configuration (retained for enterprise perMachine schema audit)
set(CPACK_GENERATOR "NSIS")
set(CPACK_WIX_UPGRADE_GUID "8F3E4A12-9B4C-4D2E-8A11-6C7F9E0D1234")
set(CPACK_WIX_INSTALL_SCOPE "perMachine")
set(CPACK_WIX_PATCH_FILE "${CMAKE_CURRENT_LIST_DIR}/windows/wix_patch.xml")
set(CPACK_WIX_PROPERTY_ALLUSERS "1")

# Standalone Native Windows .exe Installer via NSIS (supports silent enterprise install: /S)
set(CPACK_NSIS_INSTALL_ROOT "C:\\Program Files\\Adobe\\Common\\Plug-ins\\7.0")
set(CPACK_NSIS_PACKAGE_NAME "MediaCore")
set(CPACK_NSIS_DISPLAY_NAME "OpenMedia Premiere Pro & Media Encoder Plugin v2.0.0")
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
set(CPACK_NSIS_MODIFY_PATH OFF)
set(CPACK_NSIS_DEFINES "RequestExecutionLevel admin")
set(CPACK_NSIS_EXTRA_PREINSTALL_COMMANDS "
  IfFileExists '$SYSDIR\\VCRUNTIME140.dll' +3 0
    MessageBox MB_OK|MB_ICONSTOP 'Microsoft Visual C++ 2015-2022 Redistributable (x64) is missing ($SYSDIR\\VCRUNTIME140.dll not found). Please install vc_redist.x64.exe from https://aka.ms/vs/17/release/vc_redist.x64.exe before installing OpenMedia.'
    Abort
  IfFileExists '$SYSDIR\\VCRUNTIME140_1.dll' +3 0
    MessageBox MB_OK|MB_ICONSTOP 'Microsoft Visual C++ 2015-2022 Redistributable (x64) is missing ($SYSDIR\\VCRUNTIME140_1.dll not found). Please install vc_redist.x64.exe from https://aka.ms/vs/17/release/vc_redist.x64.exe before installing OpenMedia.'
    Abort
")
