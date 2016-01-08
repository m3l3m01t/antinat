!include "Sections.nsh"

; The name of the installer
Name "Antinat"
SetCompressor LZMA

; The file to write
OutFile "antinat-win32-installer.exe"

; The default installation directory
InstallDir $PROGRAMFILES\Antinat

; Registry key to check for directory (so if you install again, it will 
; overwrite the old one automatically)
InstallDirRegKey HKLM "Software\Antinat" "Install_Dir"

;--------------------------------

; Pages

Page license
Page components
Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

LicenseText "This software is licensed under the GPL, which attaches some conditions to modification and distribution (but not to use.)  Please read the full text for these conditions."
LicenseData "COPYING"

;--------------------------------

; The stuff to install
Section "Antinat (required)"

  SectionIn RO
  
  ; Set output path to the installation directory.
  SetOutPath $INSTDIR
  
  ; Put file there
  File "server\antinat.exe"
!ifndef MINGW
  File "client\antinat.dll"
!endif
  
  ; Write the installation path into the registry
  WriteRegStr HKLM SOFTWARE\Antinat "Install_Dir" "$INSTDIR"
  WriteRegStr HKLM SOFTWARE\Antinat "antinat.xml" "$INSTDIR\antinat.xml"
  WriteRegStr HKCU SOFTWARE\Antinat "antinat.xml" "$INSTDIR\antinat.xml"
  
  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Antinat" "DisplayName" "Antinat"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Antinat" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Antinat" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Antinat" "NoRepair" 1
  WriteUninstaller "uninstall.exe"

  CreateDirectory "$SMPROGRAMS\Antinat"
  CreateShortCut "$SMPROGRAMS\Antinat\Antinat.lnk" "$INSTDIR\antinat.exe" "-a" "$INSTDIR\antinat.exe" 0
  CreateShortCut "$SMPROGRAMS\Antinat\Edit Configuration.lnk" "notepad.exe" "$INSTDIR\antinat.xml" 
  CreateShortCut "$SMPROGRAMS\Antinat\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0

  CreateDirectory "$INSTDIR\html"
SectionEnd

Section "Antinat.xml help"
  SetOutPath "$INSTDIR\html"
  File "html\antinat.xml.4.html"
  File "html\antinat.1.html"
  CreateShortCut "$SMPROGRAMS\Antinat\Antinat.xml Help.lnk" "$INSTDIR\html\antinat.xml.4.html" "" "$INSTDIR\html\antinat.xml.4.html" 0
SectionEnd

Section "Allow private IPs only" g1o1
  SetOutPath $INSTDIR
  File /oname=antinat.xml "etc\antinat.xml.private"
SectionEnd

Section /o "Allow authenticated users only" g1o2
  SetOutPath $INSTDIR
  File /oname=antinat.xml "etc\antinat.xml.auth"
SectionEnd

Section /o "Allow all connections" g1o3
  SetOutPath $INSTDIR
  File /oname=antinat.xml "etc\antinat.xml.allopen"
SectionEnd

Section "Install As Service"
  ExecWait '"antinat.exe" -i'
SectionEnd

Section "Developer Support"
  SetOutPath $INSTDIR
  File "client\antinat.h"
!ifndef MINGW
  File "client\antinat.lib"
  File "client\antinatst.lib"
!else
  File "client\.libs\libantinat.a"
!endif

  SetOutPath "$INSTDIR\html"
  File "html\*.3.html"
  CreateShortCut "$SMPROGRAMS\Antinat\Antinat development.lnk" "$INSTDIR\html\antinat.3.html" "" "$INSTDIR\html\antinat.3.html" 0
SectionEnd

;--------------------------------

; Uninstaller

Section "Uninstall"

  ExecWait '"$INSTDIR\antinat.exe" -r'
  
  ; Remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Antinat"
  DeleteRegKey HKLM SOFTWARE\Antinat

  ; Remove files and uninstaller
  Delete $INSTDIR\antinat.exe
  Delete $INSTDIR\antinat.h
  Delete $INSTDIR\antinat.lib
  Delete $INSTDIR\antinatst.lib
  Delete $INSTDIR\antinat.dll
  Delete $INSTDIR\uninstall.exe
  ; Delete $INSTDIR\antinat.xml - should we do this?
  Delete "$INSTDIR\html\*.html"
  RMDir "$INSTDIR\html"

  ; Remove shortcuts, if any
  Delete "$SMPROGRAMS\Antinat\*.*"

  ; Remove directories used
  RMDir "$SMPROGRAMS\Antinat"
  RMDir "$INSTDIR"

SectionEnd

Function .onInit

  StrCpy $1 ${g1o1}

FunctionEnd

Function .onSelChange

  !insertmacro StartRadioButtons $1
    !insertmacro RadioButton ${g1o1}
    !insertmacro RadioButton ${g1o2}
    !insertmacro RadioButton ${g1o3}
  !insertmacro EndRadioButtons

FunctionEnd
