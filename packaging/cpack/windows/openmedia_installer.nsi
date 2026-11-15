; OpenMedia Premiere Pro & Media Encoder Plugin v2.0.0 — Native Windows x64 NSIS Installer
; Installs Dual-PRM Importers, Exporter, OpenMediaCore.dll, FFmpeg Runtime DLLs, and Config.toml to MediaCore 7.0

!include "MUI2.nsh"
!include "x64.nsh"

Name "OpenMedia Premiere Pro & Media Encoder Plugin v2.0.0"
OutFile "${OUTFILE}"
InstallDir "C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore"
RequestExecutionLevel admin
Unicode True
SetCompressor /SOLID lzma

!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenMediaPremierePlugin"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${STAGE_DIR}\THIRD_PARTY_LICENSES.md"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Function .onInit
  ${If} ${RunningX64}
    SetRegView 64
    ${DisableX64FSRedirection}
  ${EndIf}

  IfFileExists "$SYSDIR\VCRUNTIME140.dll" 0 MissingVcRuntime
  IfFileExists "$SYSDIR\VCRUNTIME140_1.dll" VcRuntimeOk MissingVcRuntime

MissingVcRuntime:
  IfSilent +2 0
    MessageBox MB_OK|MB_ICONSTOP "Microsoft Visual C++ 2015-2022 Redistributable (x64) is missing ($SYSDIR\VCRUNTIME140.dll or $SYSDIR\VCRUNTIME140_1.dll not found).$\r$\n$\r$\nPlease download and install vc_redist.x64.exe from:$\r$\nhttps://aka.ms/vs/17/release/vc_redist.x64.exe"
  Abort

VcRuntimeOk:
FunctionEnd

Section "OpenMedia MediaCore 7.0 Plugins" SecMain
  SectionIn RO
  SetOutPath "$INSTDIR"

  File "${STAGE_DIR}\OpenMedia_Importer.prm"
  File "${STAGE_DIR}\OpenMedia_ImporterHP.prm"
  File "${STAGE_DIR}\OpenMedia_Exporter.prm"
  File "${STAGE_DIR}\OpenMediaCore.dll"
  File /nonfatal "${STAGE_DIR}\av*.dll"
  File /nonfatal "${STAGE_DIR}\sw*.dll"
  File "${STAGE_DIR}\Config.toml"
  File "${STAGE_DIR}\THIRD_PARTY_LICENSES.md"
  File "${STAGE_DIR}\README.md"

  WriteUninstaller "$INSTDIR\Uninstall-OpenMedia.exe"

  WriteRegStr HKLM "${UNINST_KEY}" "DisplayName" "OpenMedia Premiere Pro & Media Encoder Plugin v2.0.0 (x64)"
  WriteRegStr HKLM "${UNINST_KEY}" "UninstallString" "$\"$INSTDIR\Uninstall-OpenMedia.exe$\""
  WriteRegStr HKLM "${UNINST_KEY}" "QuietUninstallString" "$\"$INSTDIR\Uninstall-OpenMedia.exe$\" /S"
  WriteRegStr HKLM "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${UNINST_KEY}" "Publisher" "OpenMedia Project"
  WriteRegStr HKLM "${UNINST_KEY}" "DisplayVersion" "2.0.0"
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoRepair" 1
SectionEnd

Section "Uninstall"
  ${If} ${RunningX64}
    SetRegView 64
    ${DisableX64FSRedirection}
  ${EndIf}

  Delete "$INSTDIR\OpenMedia_Importer.prm"
  Delete "$INSTDIR\OpenMedia_ImporterHP.prm"
  Delete "$INSTDIR\OpenMedia_Exporter.prm"
  Delete "$INSTDIR\OpenMediaCore.dll"
  Delete "$INSTDIR\av*.dll"
  Delete "$INSTDIR\sw*.dll"
  Delete "$INSTDIR\Config.toml"
  Delete "$INSTDIR\THIRD_PARTY_LICENSES.md"
  Delete "$INSTDIR\README.md"
  Delete "$INSTDIR\Uninstall-OpenMedia.exe"

  DeleteRegKey HKLM "${UNINST_KEY}"
SectionEnd
