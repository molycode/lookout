# Package dist\lookout.exe and dist\lookout.com, which tools\windows\build-release.ps1 built, as
# dist\lookout-<version>-x86_64-setup.exe with its .sha256.
#
#   tools\windows\make-installer.ps1 [-MakeNsis <makensis.exe>]
#
# makensis comes from -MakeNsis or LKT_MAKENSIS and must be NSIS's strlen_8192 build. The version comes from
# lookout.com itself, so a setup can never carry another build's number.

param(
	[string] $MakeNsis = $env:LKT_MAKENSIS
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

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

function Assert-VersionInfo([string] $path, [string] $version)
{
	$info = (Get-Item $path).VersionInfo

	if ($info.ProductName -ne 'Lookout' -or $info.ProductVersion -ne $version)
	{
		Fail "$path names '$($info.ProductName)' '$($info.ProductVersion)', not 'Lookout' '$version'"
	}
}

function Invoke-MakeNsis([string[]] $arguments)
{
	& $MakeNsis /V2 /INPUTCHARSET UTF8 /WX @arguments (Join-Path $PSScriptRoot 'lookout.nsi')

	if ($LASTEXITCODE -ne 0)
	{
		Fail "makensis failed ($LASTEXITCODE)"
	}
}

if ([string]::IsNullOrEmpty($MakeNsis) -or -not (Test-Path $MakeNsis -PathType Leaf))
{
	Fail "no makensis at '$MakeNsis' - pass -MakeNsis or set LKT_MAKENSIS to NSIS 3.13's makensis.exe"
}

# makensis wraps this output mid-word.
if (((& $MakeNsis /HDRINFO) -join '') -notmatch 'NSIS_MAX_STRLEN=8192,')
{
	Fail "$MakeNsis is not NSIS's strlen_8192 build - unpack nsis-3.13-strlen_8192.zip over it"
}

$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
Set-Location $root

$dist = Join-Path $root 'dist'
$console = Join-Path $dist 'lookout.com'

if (-not (Test-Path (Join-Path $dist 'lookout.exe')) -or -not (Test-Path $console))
{
	Fail "dist\lookout.exe and dist\lookout.com are missing - run tools\windows\build-release.ps1 first"
}

$versionLine = (& $console --version | Out-String).Trim()

if ($LASTEXITCODE -ne 0 -or $versionLine -notmatch '^Lookout (\d+\.\d+\.\d+)$')
{
	Fail "dist\lookout.com --version said '$versionLine'"
}

$version = $Matches[1]

# A published release never changes, so its version is packaged only from a clean checkout of its tag.
& git rev-parse -q --verify "refs/tags/v$version" | Out-Null

if ($LASTEXITCODE -eq 0)
{
	$isTagCommit = (& git rev-parse HEAD) -eq (& git rev-parse "v$version^{commit}")
	$isClean = [string]::IsNullOrEmpty((& git status --porcelain | Out-String).Trim())

	if (-not ($isTagCommit -and $isClean))
	{
		Fail "Lookout $version is released - package it only from a clean checkout of v$version, or rebuild with a newer version"
	}
}

Write-Host "`nPackaging Lookout $version" -ForegroundColor White

Assert-VersionInfo (Join-Path $dist 'lookout.exe') $version
Assert-VersionInfo $console $version

$name = "lookout-$version-x86_64-setup.exe"
$setup = Join-Path $dist $name

Invoke-MakeNsis @("/DVERSION=$version", "/DDIST=$dist", "/DOUTFILE=$setup")
Assert-VersionInfo $setup $version

$hash = (Get-FileHash $setup -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText("$setup.sha256", "$hash  $name`n", (New-Object Text.UTF8Encoding $false))

Write-Ok "dist\$name  $([math]::Floor((Get-Item $setup).Length / 1024)) KiB"
Write-Ok "dist\$name.sha256"
