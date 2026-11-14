# CPackConfig_Windows.cmake — Windows MSI/NSIS Installer Configuration
# Targets C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore with silent /qn support.

set(CPACK_PACKAGE_NAME "OpenMediaPremierePlugin")
set(CPACK_PACKAGE_VENDOR "OpenMedia Project")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Open-Source AV1/VP9/MKV Importer & Exporter for Adobe Premiere Pro & Media Encoder")
set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "0")
set(CPACK_PACKAGE_VERSION_PATCH "0")

set(OPENMEDIA_WIN_MEDIACORE_DIR "C:\\Program Files\\Adobe\\Common\\Plug-ins\\7.0\\MediaCore")
set(CPACK_PACKAGING_INSTALL_PREFIX "${OPENMEDIA_WIN_MEDIACORE_DIR}")

# Packaged binaries: OpenMedia_Importer.prm, OpenMedia_Exporter.prm, libOpenMediaCore.dll
set(OPENMEDIA_WINDOWS_ARTIFACTS
    "OpenMedia_Importer.prm"
    "OpenMedia_Exporter.prm"
    "libOpenMediaCore.dll"
)

# WiX MSI Generator (supports silent enterprise install: msiexec /i OpenMedia.msi /qn)
set(CPACK_GENERATOR "WIX;NSIS")
set(CPACK_WIX_UPGRADE_GUID "8F3E4A12-9B4C-4D2E-8A11-6C7F9E0D1234")
set(CPACK_WIX_INSTALL_SCOPE "perMachine")
set(CPACK_WIX_PATCH_FILE "${CMAKE_CURRENT_LIST_DIR}/windows/wix_patch.xml")
set(CPACK_WIX_PROPERTY_ALLUSERS "1")

# NSIS Installer Fallback (supports silent install: /S)
set(CPACK_NSIS_INSTALL_ROOT "C:\\Program Files\\Adobe\\Common\\Plug-ins\\7.0")
set(CPACK_NSIS_PACKAGE_NAME "MediaCore")
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
set(CPACK_NSIS_MODIFY_PATH OFF)
