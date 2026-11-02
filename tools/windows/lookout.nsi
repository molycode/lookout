; Lookout's setup and uninstaller, compiled twice by tools\windows\make-installer.ps1, so that the uninstaller can be
; signed before the setup packs it:
;   /DEXPORT_UNINST  writes the uninstaller to ${DIST}\uninstall.exe; the setup it makes is thrown away
;   /DIMPORT_UNINST  packs ${DIST}\uninstall.exe into the setup
; Both take /DVERSION=<x.y.z> /DDIST=<folder holding lookout.exe and lookout.com> /DOUTFILE=<setup to write>, and need
; NSIS's strlen_8192 build: the PATH code refuses a value it cannot hold whole.

!ifndef VERSION | DIST | OUTFILE
	!error "VERSION, DIST and OUTFILE must be defined"
!endif

!ifdef EXPORT_UNINST & IMPORT_UNINST
	!error "define EXPORT_UNINST or IMPORT_UNINST, not both"
!endif

!ifndef EXPORT_UNINST & IMPORT_UNINST
	!error "define EXPORT_UNINST or IMPORT_UNINST"
!endif

!if ${NSIS_MAX_STRLEN} < 8192
	!error "this NSIS holds strings of ${NSIS_MAX_STRLEN} characters; install the strlen_8192 build over it"
!endif

!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\Lookout"
; Lookout reads this name too: src/config/private/version_check.cpp.
!define VERSION_CHECK_OFF "version-check-off"

Unicode true
ManifestDPIAware true
ManifestSupportedOS Win10
RequestExecutionLevel user
SetCompressor /SOLID lzma
AllowSkipFiles off

Name "Lookout"
BrandingText "Lookout ${VERSION}"
OutFile "${OUTFILE}"
InstallDir "$LOCALAPPDATA\Programs\Lookout"
InstallDirRegKey HKCU "${UNINSTALL_KEY}" "InstallLocation"

VIProductVersion "${VERSION}.0"
VIFileVersion "${VERSION}.0"
VIAddVersionKey /LANG=1033 "ProductName" "Lookout"
VIAddVersionKey /LANG=1033 "ProductVersion" "${VERSION}"
VIAddVersionKey /LANG=1033 "FileVersion" "${VERSION}"
VIAddVersionKey /LANG=1033 "LegalCopyright" "© 2026 moly"
!ifdef EXPORT_UNINST
	VIAddVersionKey /LANG=1033 "FileDescription" "Lookout uninstaller"
!else
	VIAddVersionKey /LANG=1033 "FileDescription" "Lookout setup"
!endif

!include FileFunc.nsh
!include LogicLib.nsh
!include MUI2.nsh
!include nsDialogs.nsh
!include Sections.nsh
!include WinMessages.nsh
!include x64.nsh

Var PreviousDir
Var SettingsDir
Var VersionCheck
Var VersionCheckBox

!define MUI_ICON "..\..\data\windows\lookout.ico"
!define MUI_UNICON "..\..\data\windows\lookout.ico"
!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TEXT "This installs Lookout ${VERSION} for you alone; it needs no administrator rights.$\r$\n$\r$\nClick Next to continue."
!define MUI_FINISHPAGE_RUN "$INSTDIR\lookout.exe"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
Page custom VersionCheckPage VersionCheckPageLeave
!define MUI_PAGE_CUSTOMFUNCTION_PRE SkipDirectoryOnUpgrade
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!ifdef EXPORT_UNINST
	!insertmacro MUI_UNPAGE_COMPONENTS
	!insertmacro MUI_UNPAGE_INSTFILES
	!uninstfinalize 'copy /Y "%1" "${DIST}\uninstall.exe" >nul' = 0
!endif

!insertmacro MUI_LANGUAGE "English"

; Out: the setup or uninstaller ends (non-zero) unless lookout.exe and lookout.com in $INSTDIR can be written. Uses $0, $1.
!macro DefineEnsureClosed UN
	Function ${UN}EnsureClosed
		${Do}
			StrCpy $0 0

			${If} ${FileExists} "$INSTDIR\lookout.exe"
				ClearErrors
				FileOpen $1 "$INSTDIR\lookout.exe" a

				${If} ${Errors}
					StrCpy $0 1
				${Else}
					FileClose $1
				${EndIf}
			${EndIf}

			${If} ${FileExists} "$INSTDIR\lookout.com"
				ClearErrors
				FileOpen $1 "$INSTDIR\lookout.com" a

				${If} ${Errors}
					StrCpy $0 1
				${Else}
					FileClose $1
				${EndIf}
			${EndIf}

			${If} $0 = 1
				${If} ${Cmd} `MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION "Lookout is running, in a window or as a lookout command in a terminal. Close it, then click Retry." /SD IDCANCEL IDCANCEL`
					DetailPrint "Lookout is running."
					Abort
				${EndIf}
			${EndIf}
		${LoopWhile} $0 = 1
	FunctionEnd
!macroend

; In: $R9 = a folder. Out: $R0 = the user's PATH as stored, $R1 = 1 when it names $R9, $R2 = its type (0 none,
; 1 REG_SZ, 2 REG_EXPAND_SZ), $R3 = 1 when it can be changed safely, $R4 = $R0 without $R9's entries. Uses $0-$8.
!macro DefineReadPath UN
	Function ${UN}ReadPath
		StrCpy $R0 ""
		StrCpy $R1 0
		StrCpy $R2 0
		StrCpy $R3 0
		StrCpy $R4 ""

		; HKEY_CURRENT_USER, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ | RRF_NOEXPAND: the type and size in bytes.
		System::Call 'advapi32::RegGetValueW(p 0x80000001, w "Environment", w "Path", i 0x10000006, *i 0 .r1, p 0, *i 0 .r2) i .r0'

		${If} $0 = 0
			IntOp $2 $2 / 2

			${If} $2 < ${NSIS_MAX_STRLEN}
				ClearErrors
				ReadRegStr $R0 HKCU "Environment" "Path"

				${IfNot} ${Errors}
					StrCpy $R2 $1
					StrCpy $R3 1
				${EndIf}
			${EndIf}
		${ElseIf} $0 = 2
			StrCpy $R3 1
		${EndIf}

		StrCpy $3 $R0
		StrCpy $5 0

		${DoWhile} $3 != ""
			StrCpy $6 0

			${Do}
				StrCpy $7 $3 1 $6
				${IfThen} $7 == ";" ${|} ${ExitDo} ${|}
				${IfThen} $7 == "" ${|} ${ExitDo} ${|}
				IntOp $6 $6 + 1
			${Loop}

			StrCpy $7 $3 $6
			IntOp $6 $6 + 1
			StrCpy $3 $3 "" $6
			StrCpy $8 $7 "" -1

			${If} $8 == "\"
				StrCpy $8 $7 -1
			${Else}
				StrCpy $8 $7
			${EndIf}

			${If} $8 == $R9
				StrCpy $R1 1
			${ElseIf} $5 = 0
				StrCpy $R4 $7
				StrCpy $5 1
			${Else}
				StrCpy $R4 "$R4;$7"
			${EndIf}
		${Loop}

		; Windows' own PATH ends in ;, which the setup keeps when it adds an entry, so removing one restores it exactly.
		StrCpy $8 $R0 "" -1

		${If} $8 == ";"
		${AndIf} $R4 != ""
			StrCpy $R4 "$R4;"
		${EndIf}
	FunctionEnd

	; In: $R5 = the new PATH, $R2 = the type it had.
	Function ${UN}WritePath
		${If} $R5 == ""
			DeleteRegValue HKCU "Environment" "Path"
		${ElseIf} $R2 = 1
			WriteRegStr HKCU "Environment" "Path" $R5
		${Else}
			WriteRegExpandStr HKCU "Environment" "Path" $R5
		${EndIf}

		SendMessage ${HWND_BROADCAST} ${WM_SETTINGCHANGE} 0 "STR:Environment" /TIMEOUT=5000
	FunctionEnd
!macroend

; As tge-core's GetUserFolder: an absolute XDG variable wins, a relative one is ignored.
; In: $R0 = the XDG variable, $R1 = Windows' folder. Out: $R2 = the Lookout folder, or "" when there is none. Uses $0-$3.
!macro DefineUserFolder UN
	Function ${UN}UserFolder
		ReadEnvStr $R2 $R0
		StrCpy $0 $R2 1 1
		StrCpy $1 $R2 1 2
		StrCpy $2 $R2 2
		StrCpy $3 0

		${If} $0 == ":"
			${If} $1 == "\"
			${OrIf} $1 == "/"
				StrCpy $3 1
			${EndIf}
		${ElseIf} $2 == "\\"
		${OrIf} $2 == "//"
		${OrIf} $2 == "\/"
		${OrIf} $2 == "/\"
			StrCpy $3 1
		${EndIf}

		${If} $3 = 0
			StrCpy $R2 $R1
		${EndIf}

		${Do}
			StrCpy $0 $R2 1 -1

			${If} $0 == "\"
			${OrIf} $0 == "/"
				StrCpy $R2 $R2 -1
			${Else}
				${ExitDo}
			${EndIf}
		${Loop}

		${If} $R2 != ""
			System::Call 'kernel32::GetFullPathNameW(w "$R2\Lookout", i ${NSIS_MAX_STRLEN}, w .R2, p 0) i .r0'

			${If} $0 = 0
			${OrIf} $0 >= ${NSIS_MAX_STRLEN}
				StrCpy $R2 ""
			${EndIf}
		${EndIf}
	FunctionEnd
!macroend

Section "Lookout" SecLookout
	SectionIn RO
	Call EnsureClosed
	SetOutPath "$INSTDIR"
	File "${DIST}\lookout.exe"
	File "${DIST}\lookout.com"
	File "/oname=LICENSE.txt" "..\..\LICENSE"
	File "README.txt"

!ifdef IMPORT_UNINST
	File "${DIST}\uninstall.exe"
!else
	WriteUninstaller "$INSTDIR\uninstall.exe"
!endif

	CreateShortcut "$SMPROGRAMS\Lookout.lnk" "$INSTDIR\lookout.exe"

	WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayName" "Lookout"
	WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayVersion" "${VERSION}"
	WriteRegStr HKCU "${UNINSTALL_KEY}" "Publisher" "moly"
	WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\lookout.exe"
	WriteRegStr HKCU "${UNINSTALL_KEY}" "UninstallString" '"$INSTDIR\uninstall.exe"'
	WriteRegStr HKCU "${UNINSTALL_KEY}" "QuietUninstallString" '"$INSTDIR\uninstall.exe" /S'
	WriteRegStr HKCU "${UNINSTALL_KEY}" "InstallLocation" "$INSTDIR"
	WriteRegStr HKCU "${UNINSTALL_KEY}" "URLInfoAbout" "https://github.com/molycode/lookout"
	WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoModify" 1
	WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoRepair" 1
	${GetSize} "$INSTDIR" "/S=0K /G=0" $0 $1 $2
	WriteRegDWORD HKCU "${UNINSTALL_KEY}" "EstimatedSize" $0
SectionEnd

Section "Add lookout.com to PATH" SecPath
SectionEnd

Section /o "Desktop shortcut" SecDesktop
SectionEnd

; An unticked option undoes what an earlier install did, so the boxes describe the result.
Section "-Options"
	StrCpy $R9 $INSTDIR
	Call ReadPath

	${If} ${SectionIsSelected} ${SecPath}
		${If} $R1 = 0
			StrLen $0 $R0
			StrLen $1 $INSTDIR
			IntOp $0 $0 + $1
			IntOp $0 $0 + 1

			${If} $R3 = 1
			${AndIf} $0 < ${NSIS_MAX_STRLEN}
				StrCpy $1 $R0 "" -1

				${If} $R0 == ""
					StrCpy $R5 $INSTDIR
				${ElseIf} $1 == ";"
					StrCpy $R5 "$R0$INSTDIR;"
				${Else}
					StrCpy $R5 "$R0;$INSTDIR"
				${EndIf}

				Call WritePath
			${Else}
				DetailPrint "PATH not changed."
				MessageBox MB_OK|MB_ICONINFORMATION "Your PATH is too long, or of a kind Setup does not change. To run lookout in a terminal, add $INSTDIR to it yourself." /SD IDOK
			${EndIf}
		${EndIf}
	${ElseIf} $R1 = 1
		StrCpy $R5 $R4
		Call WritePath
	${EndIf}

	${If} ${SectionIsSelected} ${SecDesktop}
		CreateShortcut "$DESKTOP\Lookout.lnk" "$INSTDIR\lookout.exe"
	${Else}
		Delete "$DESKTOP\Lookout.lnk"
	${EndIf}

	${If} $SettingsDir == ""
		DetailPrint "Version check not changed."
		MessageBox MB_OK|MB_ICONINFORMATION "Setup cannot find Lookout's settings folder, so it leaves the version check as it is. To change it, click the icon at the right end of Lookout's menu bar." /SD IDOK
	${ElseIf} $VersionCheck = 0
		CreateDirectory $SettingsDir
		ClearErrors
		FileOpen $0 "$SettingsDir\${VERSION_CHECK_OFF}" w

		${If} ${Errors}
			DetailPrint "Version check not turned off."
			MessageBox MB_OK|MB_ICONEXCLAMATION "Could not write $SettingsDir\${VERSION_CHECK_OFF}, so Lookout will still check for new versions. To turn that off, click the icon at the right end of Lookout's menu bar." /SD IDOK
		${Else}
			FileClose $0
		${EndIf}
	${Else}
		Delete "$SettingsDir\${VERSION_CHECK_OFF}"

		${If} ${FileExists} "$SettingsDir\${VERSION_CHECK_OFF}"
			DetailPrint "Version check not turned on."
			MessageBox MB_OK|MB_ICONEXCLAMATION "Could not remove $SettingsDir\${VERSION_CHECK_OFF}, so Lookout will not check for new versions. To turn that on, click the icon at the right end of Lookout's menu bar." /SD IDOK
		${EndIf}
	${EndIf}
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
	!insertmacro MUI_DESCRIPTION_TEXT ${SecLookout} "The program and its command line, a Start menu entry and the uninstaller."
	!insertmacro MUI_DESCRIPTION_TEXT ${SecPath} "Lets you run lookout in cmd and PowerShell."
	!insertmacro MUI_DESCRIPTION_TEXT ${SecDesktop} "A shortcut to Lookout on your desktop."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

!insertmacro DefineEnsureClosed ""
!insertmacro DefineReadPath ""
!insertmacro DefineUserFolder ""

Function .onInit
	${IfNot} ${RunningX64}
		MessageBox MB_OK|MB_ICONSTOP "Lookout needs 64-bit Windows." /SD IDOK
		Abort
	${EndIf}

	StrCpy $R0 "XDG_CONFIG_HOME"
	StrCpy $R1 $APPDATA
	Call UserFolder
	StrCpy $SettingsDir $R2
	StrCpy $VersionCheck 1

	${If} $SettingsDir != ""
	${AndIf} ${FileExists} "$SettingsDir\${VERSION_CHECK_OFF}"
		StrCpy $VersionCheck 0
	${EndIf}

	${GetParameters} $0
	ClearErrors
	${GetOptions} $0 "/NOVERSIONCHECK" $1

	${IfNot} ${Errors}
		StrCpy $VersionCheck 0
	${EndIf}

	ReadRegStr $PreviousDir HKCU "${UNINSTALL_KEY}" "InstallLocation"

	${If} $PreviousDir != ""
	${AndIf} ${FileExists} "$PreviousDir\lookout.exe"
		StrCpy $INSTDIR $PreviousDir
		StrCpy $R9 $INSTDIR
		Call ReadPath

		${If} $R1 = 0
			!insertmacro UnselectSection ${SecPath}
		${EndIf}

		${If} ${FileExists} "$DESKTOP\Lookout.lnk"
			!insertmacro SelectSection ${SecDesktop}
		${EndIf}
	${Else}
		StrCpy $PreviousDir ""
	${EndIf}
FunctionEnd

; Shown on upgrades too, ticked as the settings folder says, so the box describes the result.
Function VersionCheckPage
	!insertmacro MUI_HEADER_TEXT "New versions" "Whether Lookout looks for newer versions of itself."
	nsDialogs::Create 1018
	Pop $0

	${If} $0 == error
		Abort
	${EndIf}

	${NSD_CreateLabel} 0 0 100% 84u "Lookout can ask GitHub (api.github.com) for its latest release when it starts and once a day, and show you when a newer one is out. The request names only Lookout and its version; like any connection, it shows GitHub your IP address. Nothing else about you or how you use Lookout is sent.$\r$\n$\r$\nYou can change this in Lookout at any time: click the icon at the right end of its menu bar."
	Pop $0
	${NSD_CreateCheckbox} 0 92u 100% 12u "Check for new versions"
	Pop $VersionCheckBox

	${If} $VersionCheck = 1
		${NSD_Check} $VersionCheckBox
	${EndIf}

	nsDialogs::Show
FunctionEnd

Function VersionCheckPageLeave
	${NSD_GetState} $VersionCheckBox $0

	${If} $0 == ${BST_CHECKED}
		StrCpy $VersionCheck 1
	${Else}
		StrCpy $VersionCheck 0
	${EndIf}
FunctionEnd

; An upgrade goes where the install is; moving it means uninstalling first.
Function SkipDirectoryOnUpgrade
	${If} $PreviousDir != ""
		Abort
	${EndIf}
FunctionEnd

; Program Files needs administrators, and ; or % would break the PATH entry.
Function .onVerifyInstDir
	StrLen $0 $INSTDIR
	StrCpy $1 0

	${DoWhile} $1 < $0
		StrCpy $2 $INSTDIR 1 $1

		${If} $2 == ";"
		${OrIf} $2 == "%"
			Abort
		${EndIf}

		IntOp $1 $1 + 1
	${Loop}

	StrLen $0 "$PROGRAMFILES32\"
	StrCpy $1 "$INSTDIR\" $0

	${If} $1 == "$PROGRAMFILES32\"
		Abort
	${EndIf}

	StrLen $0 "$PROGRAMFILES64\"
	StrCpy $1 "$INSTDIR\" $0

	${If} $1 == "$PROGRAMFILES64\"
		Abort
	${EndIf}
FunctionEnd

!ifdef EXPORT_UNINST
	Section "un.Lookout" UnSecLookout
		SectionIn RO
		Call un.EnsureClosed
		Delete "$INSTDIR\lookout.exe"
		Delete "$INSTDIR\lookout.com"
		Delete "$INSTDIR\LICENSE.txt"
		Delete "$INSTDIR\README.txt"
		Delete "$INSTDIR\uninstall.exe"
		RMDir "$INSTDIR"
		Delete "$SMPROGRAMS\Lookout.lnk"
		Delete "$DESKTOP\Lookout.lnk"

		StrCpy $R9 $INSTDIR
		Call un.ReadPath

		${If} $R1 = 1
			StrCpy $R5 $R4
			Call un.WritePath
		${ElseIf} $R3 = 0
			DetailPrint "PATH not changed."
			MessageBox MB_OK|MB_ICONINFORMATION "Your PATH is too long, or of a kind Setup does not change. If it names $INSTDIR, remove that yourself." /SD IDOK
		${EndIf}

		DeleteRegKey HKCU "${UNINSTALL_KEY}"

		StrCpy $R0 "XDG_CACHE_HOME"
		StrCpy $R1 $LOCALAPPDATA
		Call un.UserFolder

		${If} $R2 != ""
			StrCpy $R3 $R2
			StrCpy $R2 "$R3\icons"
			Call un.RemoveTree
			RMDir $R3
		${EndIf}
	SectionEnd

	Section /o "un.Settings, logs and games" UnSecPurge
		StrCpy $R0 "XDG_CONFIG_HOME"
		StrCpy $R1 $APPDATA
		Call un.UserFolder
		Call un.RemoveTree

		StrCpy $R0 "XDG_DATA_HOME"
		StrCpy $R1 $APPDATA
		Call un.UserFolder
		Call un.RemoveTree

		StrCpy $R0 "XDG_STATE_HOME"
		StrCpy $R1 $LOCALAPPDATA
		Call un.UserFolder
		Call un.RemoveTree

		StrCpy $R0 "XDG_CACHE_HOME"
		StrCpy $R1 $LOCALAPPDATA
		Call un.UserFolder
		Call un.RemoveTree
	SectionEnd

	!insertmacro MUI_UNFUNCTION_DESCRIPTION_BEGIN
		!insertmacro MUI_DESCRIPTION_TEXT ${UnSecLookout} "The program, its shortcuts, its PATH entry and the game icons it keeps."
		!insertmacro MUI_DESCRIPTION_TEXT ${UnSecPurge} "Also your settings, logs, downloaded games and your own games and protocols."
	!insertmacro MUI_UNFUNCTION_DESCRIPTION_END

	!insertmacro DefineEnsureClosed "un."
	!insertmacro DefineReadPath "un."
	!insertmacro DefineUserFolder "un."

	Function un.onInit
		${un.GetParameters} $0
		ClearErrors
		${un.GetOptions} $0 "/PURGE" $1

		${IfNot} ${Errors}
			!insertmacro SelectSection ${UnSecPurge}
		${EndIf}
	FunctionEnd

	; RMDir /r would follow a junction and empty its target; rd removes the junction itself. cmd expands a % even
	; inside quotes, so a folder named with one is left to the user. In: $R2 = the folder. Uses $0-$3.
	Function un.RemoveTree
		StrLen $0 $R2
		StrCpy $1 0
		StrCpy $2 0

		${DoWhile} $1 < $0
			StrCpy $3 $R2 1 $1
			${IfThen} $3 == "%" ${|} StrCpy $2 1 ${|}
			IntOp $1 $1 + 1
		${Loop}

		${If} $R2 != ""
		${AndIf} ${FileExists} "$R2\*.*"
			${If} $2 = 0
				nsExec::ExecToLog '"$SYSDIR\cmd.exe" /c rd /s /q "$R2"'
				Pop $0
			${EndIf}

			${If} ${FileExists} "$R2\*.*"
				DetailPrint "Could not remove $R2."
				MessageBox MB_OK|MB_ICONEXCLAMATION "Could not remove $R2; remove it yourself." /SD IDOK
			${EndIf}
		${EndIf}
	FunctionEnd
!endif
