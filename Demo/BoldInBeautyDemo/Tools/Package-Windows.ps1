<#
.SYNOPSIS
  Packages the Boldinbeauty demo (tester build) for Windows 64-bit and zips it for sharing.

.DESCRIPTION
  Runs on a WINDOWS PC with Unreal Engine 5.8 and Visual Studio 2022 installed (see docs/windows-build.md). Unreal cannot
  build the Windows target from a Mac or a Linux machine, so this has to run on Windows. It:
    1. cooks and packages the project as a Development build (NOT Shipping: the ability effects are debug-draw shapes that
       Shipping builds remove),
    2. writes a launcher (BoldInBeauty-Tester.bat) next to the game exe: guest play works without any server; when -ApiUrl is
       given the launcher also points the sign-in screen at your hosted account server,
    3. smoke-tests the packaged exe (starts it headless for a few seconds, fails on a crash or fatal error in the log),
    4. zips the result and prints the SHA-256 of the zip.

  Nothing in this script has been run on Windows yet (no AI had a Windows machine): treat the first run as the test, and send
  Build\Windows\PackageWindows.log back if anything fails.

.PARAMETER Engine
  Unreal Engine 5.8 install folder. Default: $env:UE_ROOT, else C:\Program Files\Epic Games\UE_5.8.

.PARAMETER ApiUrl
  Optional https:// address of your deployed account server (docs/hosting.md). Leave empty to ship a guest-only build.

.PARAMETER Out
  Output folder. Default: Demo\BoldInBeautyDemo\Build\Windows.

.PARAMETER SkipSmokeTest
  Skip step 3 (for example on a PC without a GPU driver).

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File Tools\Package-Windows.ps1
  powershell -ExecutionPolicy Bypass -File Tools\Package-Windows.ps1 -ApiUrl https://boldinbeauty-accounts.onrender.com
#>
[CmdletBinding()]
param(
    [string]$Engine = $(if ($env:UE_ROOT) { $env:UE_ROOT } else { 'C:\Program Files\Epic Games\UE_5.8' }),
    [string]$ApiUrl = '',
    [string]$Out = '',
    [switch]$SkipSmokeTest
)

$ErrorActionPreference = 'Stop'
$ProjectDir = Split-Path -Parent $PSScriptRoot
$Project = Join-Path $ProjectDir 'BoldInBeautyDemo.uproject'
if (-not $Out) { $Out = Join-Path $ProjectDir 'Build\Windows' }
$Stamp = Get-Date -Format 'yyyy-MM-dd'
$Log = Join-Path $Out 'PackageWindows.log'
New-Item -ItemType Directory -Force -Path $Out | Out-Null

function Say($Text) { Write-Host "[package] $Text" }
function Fail($Text) { Write-Host "[package] FAILED: $Text" -ForegroundColor Red; exit 1 }

# ---------------------------------------------------------------- checks
if (-not (Test-Path $Project)) { Fail "project not found: $Project" }
$RunUAT = Join-Path $Engine 'Engine\Build\BatchFiles\RunUAT.bat'
if (-not (Test-Path $RunUAT)) { Fail "RunUAT.bat not found under '$Engine'. Install Unreal Engine 5.8 from the Epic Games Launcher, or pass -Engine <folder>." }
if ($ApiUrl -and $ApiUrl -notmatch '^https://') { Fail "-ApiUrl must start with https:// (got '$ApiUrl')." }
$Msvc = Get-ChildItem 'C:\Program Files\Microsoft Visual Studio\2022', 'C:\Program Files (x86)\Microsoft Visual Studio\2022' -Directory -ErrorAction SilentlyContinue
if (-not $Msvc) { Say 'Visual Studio 2022 was not found in the default location. Unreal needs it with the "Game development with C++" workload; the build will stop if it is missing.' }
Say "engine: $Engine"
Say "project: $Project"
Say "output: $Out"

# ---------------------------------------------------------------- cook + package
$Archive = Join-Path $Out 'Packaged'
if (Test-Path $Archive) { Remove-Item -Recurse -Force $Archive }
$UatArgs = @(
    'BuildCookRun',
    "-project=`"$Project`"",
    '-noP4', '-utf8output', '-nocompileeditor',
    '-platform=Win64',
    '-clientconfig=Development', '-serverconfig=Development',
    '-build', '-cook', '-allmaps', '-stage', '-pak', '-compressed',
    '-prereqs',
    '-archive', "-archivedirectory=`"$Archive`""
)
Say 'cooking and packaging (this takes a while: a clean first build is 20 to 60 minutes)...'
# Windows PowerShell 5.1 turns native stderr output into a terminating error when the preference is 'Stop': relax it for the build
# and judge the result by the exit code only.
$PreviousPreference = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
& $RunUAT @UatArgs 2>&1 | Tee-Object -FilePath $Log
$UatExit = $LASTEXITCODE
$ErrorActionPreference = $PreviousPreference
if ($UatExit -ne 0) { Fail "RunUAT exited with $UatExit. See $Log (search for 'Error:')." }

# The packaged folder holds a small launcher exe at its root and the real game under <project>\Binaries\Win64.
$AllExes = @(Get-ChildItem $Archive -Recurse -Filter 'BoldInBeautyDemo.exe' -ErrorAction SilentlyContinue)
$Exe = $AllExes | Sort-Object { $_.FullName.Length } | Select-Object -First 1
$RealExe = $AllExes | Where-Object { $_.FullName -like '*Binaries\Win64*' } | Select-Object -First 1
if (-not $Exe) { Fail "packaging finished but BoldInBeautyDemo.exe was not found under $Archive" }
if (-not $RealExe) { $RealExe = $Exe }
Say "packaged: $($Exe.FullName)"

# ---------------------------------------------------------------- launcher
$Launcher = Join-Path $Exe.DirectoryName 'BoldInBeauty-Tester.bat'
$LauncherArgs = if ($ApiUrl) { "-BIBApi=$ApiUrl" } else { '' }
@"
@echo off
rem Boldinbeauty tester build. Everyone who joins (guest or signed in) is a tester: Lobby > Tester panel.
rem Guest play works offline. Progress is saved on this PC.
start "" "%~dp0BoldInBeautyDemo.exe" $LauncherArgs %*
"@ | Set-Content -Path $Launcher -Encoding ASCII
@"
Boldinbeauty - tester build (Windows 64-bit)

1. Unzip the whole folder anywhere (not inside the zip viewer).
2. Double-click BoldInBeauty-Tester.bat. Windows may show "Windows protected your PC" because the game is not code-signed:
   click "More info", then "Run anyway".
3. First screen: Play as guest (works offline) or create an account / sign in$(if ($ApiUrl) { '' } else { ' (sign-in needs a hosted account server: this build has none, so use guest)' }).
4. Everyone is a tester: in the hub open Lobby > Tester panel to set currencies, unlock operators, levels, campaign progress, gear, XP and more.
5. Esc / P pauses in a level. Close the window to quit; progress is saved automatically.

If the game does not start, install the Visual C++ runtime from Engine\Extras\Redist\en-us (UEPrereqSetup_x64.exe in this folder if present) and try again.
Problems: send the newest file from %LOCALAPPDATA%\BoldInBeautyDemo\Saved\Logs.
"@ | Set-Content -Path (Join-Path $Exe.DirectoryName 'README-TESTERS.txt') -Encoding ASCII

# ---------------------------------------------------------------- smoke test
if (-not $SkipSmokeTest) {
    Say 'smoke test: starting the packaged game headless for 30 seconds...'
    $SmokeLog = Join-Path $Out 'smoke.log'
    if (Test-Path $SmokeLog) { Remove-Item $SmokeLog }
    $Proc = Start-Process -FilePath $RealExe.FullName -PassThru -ArgumentList @('-nullrhi', '-nosound', '-unattended', '-nosplash', '-BIBGuest', '-log', "-abslog=`"$SmokeLog`"")
    Start-Sleep -Seconds 30
    $Crashed = $Proc.HasExited -and $Proc.ExitCode -ne 0
    if (-not $Proc.HasExited) { Stop-Process -Id $Proc.Id -Force }
    Get-Process -Name 'BoldInBeautyDemo*' -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    $Text = if (Test-Path $SmokeLog) { Get-Content $SmokeLog -Raw } else { '' }
    if ($Crashed) { Fail "the packaged game exited with code $($Proc.ExitCode) during the smoke test. See $SmokeLog" }
    if ($Text -match 'Fatal error|Assertion failed|Unhandled Exception') { Fail "the smoke-test log has a fatal error. See $SmokeLog" }
    if (-not $Text) { Fail "the smoke test produced no log. See $SmokeLog" }
    Say 'smoke test passed (no crash, no fatal error in 30 seconds). It does not replace playing the build once.'
}

# ---------------------------------------------------------------- zip
$Zip = Join-Path $Out "BoldInBeauty-Tester-Windows-$Stamp.zip"
if (Test-Path $Zip) { Remove-Item $Zip }
Say 'zipping...'
# tar.exe ships with Windows 10 and later and has no 2 GB limit; Compress-Archive is the fallback.
$Tar = Get-Command tar.exe -ErrorAction SilentlyContinue
if ($Tar) {
    & $Tar.Source -a -c -f $Zip -C $Exe.DirectoryName .
    if ($LASTEXITCODE -ne 0) { Fail "tar.exe could not create the zip (exit $LASTEXITCODE)." }
} else {
    Compress-Archive -Path (Join-Path $Exe.DirectoryName '*') -DestinationPath $Zip -CompressionLevel Optimal
}
$Hash = (Get-FileHash $Zip -Algorithm SHA256).Hash
$SizeMb = [math]::Round((Get-Item $Zip).Length / 1MB, 1)
Say "done: $Zip ($SizeMb MB)"
Say "SHA-256: $Hash"
Say 'Share the zip (Google Drive, itch.io, GitHub release). Testers: unzip, run BoldInBeauty-Tester.bat.'
