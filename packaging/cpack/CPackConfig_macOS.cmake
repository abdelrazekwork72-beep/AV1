# CPackConfig_macOS.cmake — macOS PKG/DMG Installer Configuration
# Targets /Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore.

set(CPACK_PACKAGE_NAME "OpenMediaPremierePlugin")
set(CPACK_PACKAGE_VENDOR "OpenMedia Project")
set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "0")
set(CPACK_PACKAGE_VERSION_PATCH "0")

set(OPENMEDIA_MACOS_MEDIACORE_DIR "/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore")
set(CPACK_PACKAGING_INSTALL_PREFIX "${OPENMEDIA_MACOS_MEDIACORE_DIR}")

# Packaged artifacts: Dual-PRM bundles (Normal + HP), Exporter bundle, libOpenMediaCore.dylib, Config.toml
set(OPENMEDIA_MACOS_ARTIFACTS
    "OpenMedia_Importer.bundle"
    "OpenMedia_ImporterHP.bundle"
    "OpenMedia_Exporter.bundle"
    "libOpenMediaCore.dylib"
    "Config.toml"
)

set(CPACK_GENERATOR "productbuild;DragNDrop")
set(CPACK_PRODUCTBUILD_IDENTIFIER "com.openmedia.premiere.pkg")
set(CPACK_PRODUCTBUILD_DISTRIBUTION_FILE "${CMAKE_CURRENT_LIST_DIR}/macos/distribution.xml")
set(CPACK_POSTFLIGHT_INSTALL_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/macos/postinstall.sh")
set(CPACK_DMG_VOLUME_NAME "OpenMedia Premiere Pro Plugins")
