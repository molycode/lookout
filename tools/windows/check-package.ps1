# Check a Lookout setup in a clean Windows: install, download and list games, start the window, refuse to replace a
# running Lookout, upgrade turning the version check off, and uninstall three ways (keeping the user's files, purging
# them, purging XDG folders).
#
#   tools\windows\check-package.ps1 [-Installer dist\lookout-<version>-x86_64-setup.exe] [-Here] [-RequireSignature]
#
# By default it runs in Windows Sandbox, which it starts. -Here runs it on this machine, which must then be Windows
# Sandbox or a CI runner: it changes the user's PATH, Start menu and installed apps. It downloads Quake II and UT2004
# from lookout-games and lists their servers, so it needs the network.

param(
	[string] $Installer,
	[switch] $Here,
	[switch] $RequireSignature,
	[string] $ResultsDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$UninstallKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\Lookout'
$SandboxTimeoutMinutes = 30

function Fail([string] $message)
{
	throw $message
}

# One Write-Host: a transcript breaks the line at -NoNewline.
function Write-Ok([string] $message)
{
	Write-Host "  $([char]0x2713) $message"
}

function Assert-Exists([string] $path, [string] $what)
{
	if (-not (Test-Path -LiteralPath $path))
	{
		Fail "$what is missing: $path"
	}
}

function Assert-Gone([string] $path, [string] $what)
{
	if (Test-Path -LiteralPath $path)
	{
		Fail "$what is still there: $path"
	}
}

# A process's own environment plus $environment; output read asynchronously, so a full pipe cannot stall it.
function Invoke-Process([string] $path, [string] $arguments, [int] $timeoutSeconds, [hashtable] $environment = @{})
{
	$info = New-Object Diagnostics.ProcessStartInfo
	$info.FileName = $path
	$info.Arguments = $arguments
	$info.UseShellExecute = $false
	$info.RedirectStandardOutput = $true
	$info.RedirectStandardError = $true
	$info.CreateNoWindow = $true

	foreach ($name in $environment.Keys)
	{
		$info.EnvironmentVariables[$name] = $environment[$name]
	}

	$process = [Diagnostics.Process]::Start($info)
	$output = $process.StandardOutput.ReadToEndAsync()
	$errors = $process.StandardError.ReadToEndAsync()

	if (-not $process.WaitForExit($timeoutSeconds * 1000))
	{
		$process.Kill()
		Fail "$path $arguments did not end within $timeoutSeconds s"
	}

	$process.WaitForExit()

	[pscustomobject]@{ ExitCode = $process.ExitCode; Output = $output.Result; Errors = $errors.Result }
}

function Get-UserPath
{
	(Get-Item 'HKCU:\Environment').GetValue('Path', '', 'DoNotExpandEnvironmentNames')
}

# Runs `lookout <arguments>` the way a new terminal would: cmd finds lookout.com through the PATH in the registry.
function Invoke-Lookout([string] $arguments, [int] $timeoutSeconds, [hashtable] $environment = @{})
{
	$machinePath = (Get-Item 'HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\Environment').GetValue('Path', '')
	$path = [Environment]::ExpandEnvironmentVariables("$machinePath;$(Get-UserPath)")
	$all = @{ Path = $path }

	foreach ($name in $environment.Keys)
	{
		$all[$name] = $environment[$name]
	}

	Invoke-Process $env:ComSpec "/d /c lookout $arguments" $timeoutSeconds $all
}

function Invoke-Setup([string] $setup, [string] $options = '')
{
	(Invoke-Process $setup "/S $options".TrimEnd() 120).ExitCode
}

function Start-Window([string] $program)
{
	$process = Start-Process -FilePath $program -PassThru

	# Windows PowerShell reports no exit code for a process whose handle it never opened.
	$null = $process.Handle
	$deadline = (Get-Date).AddSeconds(30)

	do
	{
		Start-Sleep -Milliseconds 250
		$process.Refresh()
	}
	while ($process.MainWindowHandle -eq [IntPtr]::Zero -and -not $process.HasExited -and (Get-Date) -lt $deadline)

	if ($process.MainWindowHandle -eq [IntPtr]::Zero)
	{
		Fail "lookout.exe showed no window within 30 s"
	}

	$process
}

function Stop-Window([Diagnostics.Process] $process)
{
	$process.CloseMainWindow() | Out-Null

	if (-not $process.WaitForExit(15000))
	{
		$process.Kill()
		Fail "lookout.exe did not close within 15 s"
	}

	if ($process.ExitCode -ne 0)
	{
		Fail "lookout.exe exited $($process.ExitCode) when closed"
	}
}

function Get-FileState([string] $path)
{
	$item = Get-Item -LiteralPath $path
	"$($item.Length) $($item.LastWriteTimeUtc.Ticks) $((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash)"
}

function Invoke-Checks([string] $setup, [string] $version)
{
	if (Test-Path $UninstallKey)
	{
		Fail "Lookout is already installed here"
	}

	$dir = Join-Path $env:LOCALAPPDATA 'Programs\Lookout'
	$startMenu = Join-Path ([Environment]::GetFolderPath('Programs')) 'Lookout.lnk'
	$desktop = Join-Path ([Environment]::GetFolderPath('Desktop')) 'Lookout.lnk'
	$userData = Join-Path $env:APPDATA 'Lookout'
	$userLocal = Join-Path $env:LOCALAPPDATA 'Lookout'
	$versionCheckOff = Join-Path $userData 'version-check-off'
	$work = Join-Path ([IO.Path]::GetTempPath()) "lookout-check-$PID"
	New-Item -ItemType Directory -Force $work | Out-Null

	if ($RequireSignature)
	{
		$status = (Get-AuthenticodeSignature -LiteralPath $setup).Status

		if ($status -ne 'Valid')
		{
			Fail "the setup's signature is $status"
		}

		Write-Ok "the setup is signed"
	}

	# Longer than NSIS's default 1024 characters, ending in ; as Windows' own default does.
	$original = (Get-UserPath).TrimEnd(';')
	$fillers = (1..40 | ForEach-Object { "C:\lookout-check\an-entry-that-only-makes-the-path-long-$_" }) -join ';'
	$planted = "$(if ($original -ne '') { "$original;" })$fillers;"
	[Microsoft.Win32.Registry]::SetValue('HKEY_CURRENT_USER\Environment', 'Path', $planted, 'ExpandString')

	$code = Invoke-Setup $setup

	if ($code -ne 0)
	{
		Fail "the setup exited $code"
	}

	foreach ($file in 'lookout.exe', 'lookout.com', 'LICENSE.txt', 'README.txt', 'uninstall.exe')
	{
		Assert-Exists (Join-Path $dir $file) $file
	}

	$key = Get-ItemProperty $UninstallKey

	if ($key.DisplayVersion -ne $version -or $key.InstallLocation -ne $dir -or $key.Publisher -ne 'moly')
	{
		Fail "the uninstall entry says $($key.DisplayVersion) in $($key.InstallLocation) by $($key.Publisher)"
	}

	Assert-Exists $startMenu 'the Start menu shortcut'
	Assert-Gone $desktop 'an unasked desktop shortcut'
	Assert-Gone $versionCheckOff 'an unasked version-check-off'

	if ((Get-UserPath) -ne "$planted$dir;")
	{
		Fail "PATH is '$(Get-UserPath)', not the planted value with '$dir;' added"
	}

	Write-Ok "installed in ${dir}: files, uninstall entry, Start menu shortcut, PATH entry after $($planted.Length) characters, version check on"

	if ($RequireSignature)
	{
		foreach ($file in 'lookout.exe', 'lookout.com', 'uninstall.exe')
		{
			$status = (Get-AuthenticodeSignature -LiteralPath (Join-Path $dir $file)).Status

			if ($status -ne 'Valid')
			{
				Fail "$file's signature is $status"
			}
		}

		Write-Ok "lookout.exe, lookout.com and uninstall.exe are signed"
	}

	$result = Invoke-Lookout '--version' 30

	if ($result.ExitCode -ne 0 -or $result.Output.Trim() -ne "Lookout $version")
	{
		Fail "lookout --version in a new cmd said '$($result.Output.Trim())' ($($result.ExitCode)): $($result.Errors)"
	}

	Write-Ok "a new cmd runs lookout.com for lookout: $($result.Output.Trim())"

	# One game on a UDP master and one on a TCP master, each through its own protocol script.
	$result = Invoke-Lookout '--download quake2 ut2004' 300

	if ($result.ExitCode -ne 0)
	{
		Fail "lookout --download quake2 ut2004 exited $($result.ExitCode): $($result.Errors)"
	}

	$listed = @()

	foreach ($game in 'quake2', 'ut2004')
	{
		$result = Invoke-Lookout "--list $game" 120
		$lines = @($result.Output -split "`r?`n" | Where-Object { $_ -ne '' })

		if ($result.ExitCode -ne 0 -or $lines.Count -eq 0)
		{
			Fail "lookout --list $game exited $($result.ExitCode) with $($lines.Count) lines: $($result.Errors)"
		}

		$listed += "$($lines.Count) $game"
	}

	Write-Ok "downloaded Quake II and UT2004, listed $($listed -join ' and ')"

	$window = Start-Window (Join-Path $dir 'lookout.exe')
	Stop-Window $window

	$log = Get-ChildItem (Join-Path $userLocal 'logs') -Filter '*.log' | Sort-Object LastWriteTimeUtc | Select-Object -Last 1

	if ($null -eq $log)
	{
		Fail "lookout.exe wrote no log"
	}

	$display = Select-String -LiteralPath $log.FullName -Pattern "video driver '[^']*', renderer '[^']*'" | Select-Object -Last 1

	if ($null -eq $display)
	{
		Fail "the log names no video driver and renderer"
	}

	Write-Ok "the window starts and closes: $($display.Matches[0].Value)"

	$window = Start-Window (Join-Path $dir 'lookout.exe')
	$before = Get-FileState (Join-Path $dir 'lookout.exe')
	$code = Invoke-Setup $setup
	$after = Get-FileState (Join-Path $dir 'lookout.exe')
	Stop-Window $window

	if ($code -eq 0 -or $before -ne $after)
	{
		Fail "the setup exited $code over a running Lookout, and lookout.exe $(if ($before -eq $after) { 'stayed' } else { 'changed' })"
	}

	Write-Ok "the setup refuses to replace a running Lookout (exit $code)"

	$code = Invoke-Setup $setup '/NOVERSIONCHECK'

	if ($code -ne 0 -or (Get-UserPath) -ne "$planted$dir;" -or -not (Test-Path $startMenu) -or (Test-Path $desktop))
	{
		Fail "the upgrade exited $code, or changed PATH or the shortcuts"
	}

	Assert-Exists $versionCheckOff 'version-check-off after /NOVERSIONCHECK'
	Write-Ok "an upgrade over the install keeps PATH and the shortcuts as they were; /NOVERSIONCHECK turns the version check off"

	$iconCache = Join-Path $userLocal 'icons'
	New-Item -ItemType Directory -Force $iconCache | Out-Null
	Set-Content -LiteralPath (Join-Path $iconCache 'seed.png') -Value ''

	# As Apps & Features does it: the uninstaller copies itself away and returns at once.
	$code = (Invoke-Process (Join-Path $dir 'uninstall.exe') '/S' 60).ExitCode
	$deadline = (Get-Date).AddSeconds(60)

	while (((Test-Path $dir) -or (Test-Path $UninstallKey) -or (Test-Path $iconCache)) -and (Get-Date) -lt $deadline)
	{
		Start-Sleep -Milliseconds 500
	}

	if ($code -ne 0)
	{
		Fail "uninstall.exe /S exited $code"
	}

	Assert-Gone $dir 'the install folder'
	Assert-Gone $UninstallKey 'the uninstall entry'
	Assert-Gone $startMenu 'the Start menu shortcut'
	Assert-Gone $iconCache 'the icon cache'
	Assert-Exists (Join-Path $userData 'config.json') 'the settings'
	Assert-Exists $versionCheckOff 'version-check-off'
	Assert-Exists (Join-Path $userData 'downloaded\games\quake2') 'the downloaded Quake II'
	Assert-Exists (Join-Path $userLocal 'logs') 'the logs'

	if ((Get-UserPath) -ne $planted)
	{
		Fail "after uninstalling, PATH is '$(Get-UserPath)', not the planted value"
	}

	Write-Ok "uninstalled: program, entry, shortcut, PATH entry and icon cache gone; settings, logs and games kept"

	$code = Invoke-Setup $setup

	if ($code -ne 0)
	{
		Fail "installing again exited $code"
	}

	Assert-Exists $versionCheckOff 'version-check-off after installing again'
	Write-Ok "installing again keeps the version check off"

	$kept = Join-Path $work 'kept'
	New-Item -ItemType Directory -Force $kept | Out-Null
	Set-Content -LiteralPath (Join-Path $kept 'kept.txt') -Value 'not Lookout''s'
	New-Item -ItemType Directory -Force (Join-Path $userData 'games') | Out-Null
	New-Item -ItemType Junction -Path (Join-Path $userData 'games\linked') -Target $kept | Out-Null

	# Run in place (_?=), so its exit code is the uninstall's own; a copy, since it deletes the original.
	$copy = Join-Path $work 'uninstall.exe'
	Copy-Item -LiteralPath (Join-Path $dir 'uninstall.exe') $copy
	$code = (Invoke-Process $copy "/S /PURGE _?=$dir" 120).ExitCode

	if ($code -ne 0)
	{
		Fail "uninstall.exe /S /PURGE exited $code"
	}

	Assert-Gone $dir 'the install folder'
	Assert-Gone $userData 'the settings and games'
	Assert-Gone $userLocal 'the logs'
	Assert-Exists (Join-Path $kept 'kept.txt') 'the target of a junction inside the games'
	Write-Ok "purged: settings, logs and games gone; a junction inside removed without entering it"

	$code = Invoke-Setup $setup

	if ($code -ne 0)
	{
		Fail "installing a third time exited $code"
	}

	$xdgRoot = Join-Path $work 'xdg'
	$xdg = @{
		XDG_CONFIG_HOME = Join-Path $xdgRoot 'config'
		XDG_DATA_HOME = Join-Path $xdgRoot 'data'
		XDG_STATE_HOME = (Join-Path $xdgRoot 'state').Replace('\', '/')
		XDG_CACHE_HOME = Join-Path $xdgRoot 'cache'
	}
	$result = Invoke-Lookout '--download quake2' 300 $xdg

	if ($result.ExitCode -ne 0)
	{
		Fail "lookout --download quake2 with XDG folders exited $($result.ExitCode): $($result.Errors)"
	}

	$xdgFolders = @('config', 'data', 'state', 'cache') | ForEach-Object { Join-Path (Join-Path $xdgRoot $_) 'Lookout' }
	New-Item -ItemType Directory -Force (Join-Path $xdgFolders[3] 'icons') | Out-Null

	foreach ($folder in $xdgFolders)
	{
		Assert-Exists $folder 'an XDG folder of Lookout'
	}

	New-Item -ItemType Directory -Force $userData | Out-Null
	Set-Content -LiteralPath (Join-Path $userData 'config.json') -Value '{}'

	Copy-Item -LiteralPath (Join-Path $dir 'uninstall.exe') $copy -Force
	$code = (Invoke-Process $copy "/S /PURGE _?=$dir" 120 $xdg).ExitCode

	if ($code -ne 0)
	{
		Fail "uninstall.exe /S /PURGE with XDG folders exited $code"
	}

	foreach ($folder in $xdgFolders)
	{
		Assert-Gone $folder 'an XDG folder of Lookout'
	}

	Assert-Exists (Join-Path $userData 'config.json') 'the settings outside the XDG folders'
	Write-Ok "purged with XDG folders set: those removed, %APPDATA%\Lookout left alone"
}

function Get-SandboxIds
{
	(& wsb list --raw | Out-String | ConvertFrom-Json).WindowsSandboxEnvironments | ForEach-Object { $_.Id }
}

function Invoke-InSandbox([string] $setup)
{
	$sandbox = Join-Path $env:windir 'System32\WindowsSandbox.exe'

	if (-not (Test-Path $sandbox))
	{
		Fail "Windows Sandbox is not enabled - turn on the Windows feature Containers-DisposableClientVM"
	}

	if ($null -eq (Get-Command wsb -ErrorAction SilentlyContinue))
	{
		Fail "no wsb command - Windows Sandbox's command line comes with Windows 11 24H2"
	}

	$results = Join-Path ([IO.Path]::GetTempPath()) "lookout-check-$PID"
	New-Item -ItemType Directory -Force $results | Out-Null
	Unblock-File -LiteralPath $setup

	$setupName = Split-Path $setup -Leaf
	$command = "powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\check\tools\check-package.ps1 -Installer C:\check\setup\$setupName -Here -ResultsDir C:\check\results"

	if ($RequireSignature)
	{
		$command += ' -RequireSignature'
	}

	$config = @"
<Configuration>
  <vGPU>Disable</vGPU>
  <Networking>Default</Networking>
  <MappedFolders>
    <MappedFolder><HostFolder>$PSScriptRoot</HostFolder><SandboxFolder>C:\check\tools</SandboxFolder><ReadOnly>true</ReadOnly></MappedFolder>
    <MappedFolder><HostFolder>$(Split-Path $setup)</HostFolder><SandboxFolder>C:\check\setup</SandboxFolder><ReadOnly>true</ReadOnly></MappedFolder>
    <MappedFolder><HostFolder>$results</HostFolder><SandboxFolder>C:\check\results</SandboxFolder><ReadOnly>false</ReadOnly></MappedFolder>
  </MappedFolders>
  <LogonCommand><Command>$command</Command></LogonCommand>
</Configuration>
"@
	$wsb = Join-Path $results 'check.wsb'
	[IO.File]::WriteAllText($wsb, $config)

	Write-Host "  Windows Sandbox runs the checks; this waits up to $SandboxTimeoutMinutes min" -ForegroundColor DarkGray
	$running = @(Get-SandboxIds)
	Start-Process -FilePath $sandbox -ArgumentList "`"$wsb`""

	$exitFile = Join-Path $results 'exit-code.txt'
	$deadline = (Get-Date).AddMinutes($SandboxTimeoutMinutes)

	while (-not (Test-Path $exitFile) -and (Get-Date) -lt $deadline)
	{
		Start-Sleep -Seconds 5
	}

	# Stopped from here: a sandbox that shuts itself down leaves an error dialog on the desktop.
	foreach ($id in Get-SandboxIds)
	{
		if ($running -notcontains $id)
		{
			& wsb stop --id $id | Out-Null
		}
	}

	$log = Join-Path $results 'check.log'

	if (Test-Path $log)
	{
		Get-Content -LiteralPath $log | Write-Host
	}

	if (-not (Test-Path $exitFile))
	{
		Fail "Windows Sandbox wrote no result within $SandboxTimeoutMinutes min (another sandbox open? closed early?)"
	}

	[int] (Get-Content -LiteralPath $exitFile -Raw).Trim()
}

$exitCode = 1
$isTranscribing = $false

try
{
	if ($Here -and -not [string]::IsNullOrEmpty($ResultsDir))
	{
		Start-Transcript -LiteralPath (Join-Path $ResultsDir 'check.log') | Out-Null
		$isTranscribing = $true
	}

	if ([string]::IsNullOrEmpty($Installer))
	{
		$newest = Get-ChildItem (Join-Path $PSScriptRoot '..\..\dist') -Filter 'lookout-*-x86_64-setup.exe' -ErrorAction SilentlyContinue |
			Sort-Object LastWriteTimeUtc | Select-Object -Last 1

		if ($null -eq $newest)
		{
			Fail "no dist\lookout-*-x86_64-setup.exe - run tools\windows\make-installer.ps1 first, or pass -Installer"
		}

		$Installer = $newest.FullName
	}

	$setup = (Resolve-Path -LiteralPath $Installer).Path

	if ((Split-Path $setup -Leaf) -notmatch '^lookout-(\d+\.\d+\.\d+)-x86_64-setup\.exe$')
	{
		Fail "$setup is not named lookout-<version>-x86_64-setup.exe"
	}

	$version = $Matches[1]

	if ($Here)
	{
		if ($env:USERNAME -ne 'WDAGUtilityAccount' -and $env:GITHUB_ACTIONS -ne 'true')
		{
			Fail "-Here changes this user's PATH, Start menu and installed apps; it runs only in Windows Sandbox or on a CI runner"
		}

		Write-Host "`nChecking $(Split-Path $setup -Leaf)" -ForegroundColor White
		Invoke-Checks $setup $version
		$exitCode = 0
	}
	else
	{
		Write-Host "`nChecking $(Split-Path $setup -Leaf) in Windows Sandbox" -ForegroundColor White
		$exitCode = Invoke-InSandbox $setup
	}
}
catch
{
	Write-Host "`n$([char]0x2717) $($_.Exception.Message)" -ForegroundColor Red
	$exitCode = 1
}
finally
{
	if ($isTranscribing)
	{
		Stop-Transcript | Out-Null
	}

	if ($Here -and -not [string]::IsNullOrEmpty($ResultsDir))
	{
		[IO.File]::WriteAllText((Join-Path $ResultsDir 'exit-code.txt'), "$exitCode")
	}
}

exit $exitCode
