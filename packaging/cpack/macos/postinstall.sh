#!/usr/bin/env bash
set -euo pipefail

# Hardened macOS postinstall script (TM-4.2.1 CWE-732 & TM-4.2.2 CWE-78)
readonly MEDIACORE_DIR="/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore"

if [[ ! -d "${MEDIACORE_DIR}" ]]; then
  exit 0
fi

for bundle in "OpenMedia_Importer.bundle" "OpenMedia_ImporterHP.bundle" "OpenMedia_Exporter.bundle"; do
  bundle_path="${MEDIACORE_DIR}/${bundle}"
  if [[ -d "${bundle_path}" ]]; then
    chmod 0755 "${bundle_path}"
    find "${bundle_path}" -type d -exec chmod 0755 {} +
    find "${bundle_path}" -type f -exec chmod 0644 {} +
    if [[ -d "${bundle_path}/Contents/MacOS" ]]; then
      find "${bundle_path}/Contents/MacOS" -type f -exec chmod 0755 {} +
    fi
  fi
done

if [[ -f "${MEDIACORE_DIR}/libOpenMediaCore.dylib" ]]; then
  chmod 0755 "${MEDIACORE_DIR}/libOpenMediaCore.dylib"
fi

if [[ -f "${MEDIACORE_DIR}/Config.toml" ]]; then
  chmod 0644 "${MEDIACORE_DIR}/Config.toml"
fi
