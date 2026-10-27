; AiBo 4.3.3 graphical installer. GPL-2.0-or-later.
Unicode true
!include "MUI2.nsh"
!ifndef STAGE
  !error "Build through tools/build_installer.py so plugins.ini is validated first."
!endif
!ifndef OUTPUT
  !error "OUTPUT must be set."
!endif
Name "AiBo 4.3.3 - Artificial Intelligence Bot"
OutFile "${OUTPUT}"
InstallDir "$PROGRAMFILES32\Steam\steamapps\common\Half-Life\"
RequestExecutionLevel highest
SetCompressor /SOLID lzma
CRCCheck force
ShowInstDetails show
BrandingText "AiBo - Artificial Intelligence Bot"
VIProductVersion "4.3.3.0"
VIAddVersionKey /LANG=1033 "ProductName" "AiBo"
VIAddVersionKey /LANG=1033 "FileDescription" "AiBo 4.3.3 Setup"
VIAddVersionKey /LANG=1033 "CompanyName" "AiBo contributors"
VIAddVersionKey /LANG=1033 "FileVersion" "4.3.3"
VIAddVersionKey /LANG=1033 "LegalCopyright" "GPL-2.0-or-later; original component notices included"
!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TITLE "AiBo 4.3.3"
!define MUI_WELCOMEPAGE_TEXT "Artificial Intelligence Bot untuk Counter-Strike 1.6.$\r$\n$\r$\nHUMAN AIM level 100 dan semua teknik menembak aktif.$\r$\n$\r$\nTutup CS dan HLDS, lalu pilih folder game atau folder cstrike. Installer memeriksa DLL, mencadangkan file yang diubah, dan menyesuaikan plugins.ini."
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${STAGE}\LICENSE.txt"
!define MUI_DIRECTORYPAGE_TEXT_TOP "Pilih folder Counter-Strike/Half-Life yang memiliki subfolder cstrike, atau pilih folder cstrike langsung."
!define MUI_PAGE_CUSTOMFUNCTION_LEAVE ValidateSelectedGame
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_TEXT "AiBo sudah terpasang. Buka console game: meta list, lalu aibo version dan aibo_human_aim_status.$\r$\n$\r$\nAtur server 32 slot untuk 31 bot dan satu manusia. Semiclip/no-block perlu mengizinkan collision untuk boost fisik."
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_LANGUAGE "English"
Section "AiBo" SEC_MAIN
  SetOutPath "$PLUGINSDIR"
  File "${STAGE}\AiBoInstall.exe"
  SetOutPath "$PLUGINSDIR\payload"
  File /r "${STAGE}\payload\*.*"
  DetailPrint "Memeriksa AiBo.dll, HUMAN AIM, dan plugins.ini..."
  ExecWait '"$PLUGINSDIR\AiBoInstall.exe" --install "$PLUGINSDIR\payload" "$INSTDIR" "$PLUGINSDIR\result.txt"' $0
  FileOpen $1 "$PLUGINSDIR\result.txt" r
  FileRead $1 $2
  FileClose $1
  DetailPrint "$2"
  StrCmp $0 0 success
  SetErrorLevel 1
  MessageBox MB_OK|MB_ICONSTOP "$2"
  Abort
success:
  SetErrorLevel 0
SectionEnd

; Validate on Next, never inside the native Browse dialog callback.
Function ValidateSelectedGame
  IfFileExists "$INSTDIR\liblist.gam" valid_game
  IfFileExists "$INSTDIR\cstrike\liblist.gam" valid_game
  MessageBox MB_OK|MB_ICONEXCLAMATION "Folder yang dipilih belum memiliki liblist.gam. Pilih folder cstrike yang berisi liblist.gam, atau folder induknya."
  Abort
valid_game:
FunctionEnd
