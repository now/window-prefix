SetCompressor /SOLID lzma

Name "Window*"
OutFile "installer/Setup.exe"
InstallDir "$PROGRAMFILES\Window Prefix"
InstallDirRegKey HKLM "Software\Window Prefix" "Installation Directory"
RequestExecutionLevel admin
ShowInstDetails show
XPStyle on
AllowSkipFiles off
VIAddVersionKey "ProductName" "Window*"
VIAddVersionKey "FileDescription" "Remove pesky dialogs by sending key sequences to dismiss them"
VIAddVersionKey "CompanyName" "bitwi.se"
VIAddVersionKey "FileVersion" "1.0.0"
VIAddVersionKey "ProductVersion" "1.0.0"
VIAddVersionKey "LegalCopyright" "Nikolai Weibull 2008"
VIProductVersion "1.0.0.0"
BrandingText " "

Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

Section "-Kill Running Window*"
killer_loop: ; Hi hi :-)
  Push $R0
  FindProcDLL::FindProc "window-prefix.exe"
  IntCmp $R0 1 ask_to_retry done_killing done_killing

ask_to_retry:
  Pop $R0
  MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION "Window* seems to be running.  Please close it and retry." /SD IDCANCEL IDRETRY killer_loop
  Abort "Abort: Installation cancelled by user."

done_killing:
  Pop $R0
SectionEnd

Var /GLOBAL Name

Section "-Install"
  StrCpy $Name "Window Prefix"

  SetOutPath $INSTDIR

  File Release\hook.dll
  File Release\window-prefix.exe

  CreateShortCut "$SMSTARTUP\$Name.lnk" "$INSTDIR\window-prefix.exe"

  ; Factor this out into a function
  WriteRegStr HKLM "Software\$Name" "Installation Directory" $INSTDIR

  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\$Name" "DisplayName" "$Name"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\$Name" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\$Name" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\$Name" "NoRepair" 1

  WriteUninstaller "uninstall.exe"

  Exec '"$INSTDIR\window-prefix.exe"'
SectionEnd

Section "Uninstall"
  StrCpy $Name "Window Prefix"

  Delete "$SMSTARTUP\$Name.lnk"

  Processes::KillProcess "window-prefix"

  Sleep 500

  Delete "$INSTDIR\window-prefix.exe"
  Delete "$INSTDIR\hook.dll"

  Delete $INSTDIR\uninstall.exe

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\$Name"
  DeleteRegKey HKLM "Software\$Name"
SectionEnd
