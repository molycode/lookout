# Build the release programs dist\lookout.exe and dist\lookout.com, the ones the installer ships.
#
#   tools\windows\build-release.ps1 [-CMake <cmake.exe>]
#
# CMake comes from -CMake or LKT_CMAKE; Visual Studio is found through vswhere, so any prompt will do.

param(
	[string] $CMake = $env:LKT_CMAKE
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Every DLL the programs may import: Windows' own. A new one is a decision, not an accident.
$AllowedDlls = @('advapi32.dll', 'bcrypt.dll', 'crypt32.dll', 'gdi32.dll', 'imm32.dll', 'kernel32.dll', 'ole32.dll',
	'setupapi.dll', 'shell32.dll', 'user32.dll', 'version.dll', 'winmm.dll', 'ws2_32.dll')

function Fail([string] $message)
{
	Write-Host "`n$([char]0x2717) $message" -ForegroundColor Red
	exit 1
}

function Write-Ok([string] $message)
{
	Write-Host "  $([char]0x2713) " -ForegroundColor Green -NoNewline
	Write-Host $message
}

# Absolute paths in a program, as UTF-8 and as UTF-16 at either alignment.
function Get-EmbeddedPaths([string] $path)
{
	$bytes = [IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $path).Path)
	$texts = @([Text.Encoding]::UTF8.GetString($bytes), [Text.Encoding]::Unicode.GetString($bytes),
		[Text.Encoding]::Unicode.GetString($bytes, 1, $bytes.Length - 1))
	$paths = foreach ($text in $texts)
	{
		[regex]::Matches($text, '[A-Za-z]:\\(?:[\p{L}\p{N} ._()+~#&$@!'',;=\[\]{}%^-]+\\)+[\p{L}\p{N} ._()+~#&$@!'',;=\[\]{}%^-]+') | ForEach-Object { $_.Value }
	}

	@($paths | Sort-Object -Unique)
}

# Windows tells a window program from a console one by the subsystem in its PE header: 2 and 3.
function Get-Subsystem([string] $path)
{
	$bytes = [IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $path).Path)
	[BitConverter]::ToUInt16($bytes, [BitConverter]::ToInt32($bytes, 0x3C) + 0x5C)
}

if ([string]::IsNullOrEmpty($CMake) -or -not (Test-Path $CMake -PathType Leaf))
{
	Fail "no cmake at '$CMake' - pass -CMake or set LKT_CMAKE to an unpacked CMake 4.3+ release's cmake.exe"
}

$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
Set-Location $root

$versionMatch = Select-String -Path CMakeLists.txt -Pattern '^project\(Lookout VERSION (\d+\.\d+\.\d+) '

if ($null -eq $versionMatch)
{
	Fail "CMakeLists.txt's project() names no version"
}

$version = $versionMatch.Matches[0].Groups[1].Value

# A published release never changes, so its version is built only from a clean checkout of its tag.
& git rev-parse -q --verify "refs/tags/v$version" | Out-Null

if ($LASTEXITCODE -eq 0)
{
	$isTagCommit = (& git rev-parse HEAD) -eq (& git rev-parse "v$version^{commit}")
	$isClean = [string]::IsNullOrEmpty((& git status --porcelain | Out-String).Trim())

	if (-not ($isTagCommit -and $isClean))
	{
		Fail "Lookout $version is released - build it only from a clean checkout of v$version, or raise the version in CMakeLists.txt's project()"
	}
}

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath

if ([string]::IsNullOrEmpty($vs))
{
	Fail "no Visual Studio with the x64 C++ tools found"
}

$vcvars = "$vs\VC\Auxiliary\Build\vcvars64.bat"
$buildDir = 'build\release'
$ctest = Join-Path (Split-Path $CMake) 'ctest.exe'

Write-Host "`nBuilding the release" -ForegroundColor White
Write-Host "  $vs" -ForegroundColor DarkGray

# The preset's toolchain and flags, in a folder of its own so a developer's msvc-Release build is left alone.
cmd /c "`"$vcvars`" >nul 2>&1 && `"$CMake`" --preset windows-msvc-release -B $buildDir --fresh -DLKT_STATIC_RUNTIME=ON -DLKT_AUTO_INIT_SUBMODULES=OFF && `"$CMake`" --build $buildDir"

if ($LASTEXITCODE -ne 0)
{
	Fail "the build failed ($LASTEXITCODE)"
}

& $ctest --test-dir $buildDir --output-on-failure

if ($LASTEXITCODE -ne 0)
{
	Fail "the tests failed ($LASTEXITCODE)"
}

$programs = @("$buildDir\lookout.exe", "$buildDir\lookout.com")

foreach ($program in $programs)
{
	$dumped = cmd /c "`"$vcvars`" >nul 2>&1 && dumpbin /nologo /dependents `"$program`""

	if ($LASTEXITCODE -ne 0)
	{
		Fail "dumpbin cannot read $program ($LASTEXITCODE)"
	}

	# Both the normal and the delay-load imports are listed as indented DLL names.
	$dlls = @($dumped | ForEach-Object { if ($_ -match '^\s+(\S+\.dll)\s*$') { $Matches[1].ToLowerInvariant() } } | Sort-Object -Unique)

	if ($dlls.Count -eq 0)
	{
		Fail "dumpbin lists no imports of $program"
	}

	foreach ($dll in $dlls)
	{
		if ($AllowedDlls -notcontains $dll)
		{
			Fail "$program imports $dll, which is not on the allowlist"
		}
	}

	Write-Ok "$(Split-Path $program -Leaf) imports only: $($dlls -join ' ')"
}

if ((Get-Subsystem $programs[0]) -ne 2)
{
	Fail "lookout.exe is not a window program"
}

if ((Get-Subsystem $programs[1]) -ne 3)
{
	Fail "lookout.com is not a console program"
}

Write-Ok "lookout.exe is a window program, lookout.com a console one"

# SDL's asserts keep their source files' absolute paths, so a build in a profile folder would publish whose it is.
$profileDir = "$env:USERPROFILE".TrimEnd('\')

foreach ($program in $programs)
{
	$paths = @(Get-EmbeddedPaths $program)
	$private = @($paths | Where-Object { $_ -match '\\Users\\' -or ($profileDir -ne '' -and $_.StartsWith($profileDir, [StringComparison]::OrdinalIgnoreCase)) -or (($_ -split '\\') -contains $env:USERNAME) })

	if ($private.Count -ne 0)
	{
		Fail "$program names a profile folder or the user $env:USERNAME, so who built it would be published: $($private[0..2] -join '; ') - build in a folder outside your profile"
	}

	$roots = @($paths | ForEach-Object { (($_ -split '\\') | Select-Object -First 3) -join '\' } | Sort-Object -Unique)
	Write-Ok "$(Split-Path $program -Leaf) names no profile folder or user ($($paths.Count) paths$(if ($roots.Count -ne 0) { ', under ' + ($roots -join ', ') }))"
}

$versionLine = (& $programs[1] --version | Out-String).Trim()

if ($LASTEXITCODE -ne 0 -or $versionLine -ne "Lookout $version")
{
	Fail "lookout.com --version said '$versionLine' ($LASTEXITCODE), not 'Lookout $version'"
}

Write-Ok $versionLine

New-Item -ItemType Directory -Force dist | Out-Null

foreach ($program in $programs)
{
	Copy-Item $program dist -Force
	Write-Ok "dist\$(Split-Path $program -Leaf)  $([math]::Floor((Get-Item $program).Length / 1024)) KiB"
}
