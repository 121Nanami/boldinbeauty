# TASK FOR THE AI ON THIS WINDOWS PC: build the Boldinbeauty tester demo

Read all of this before doing anything. Do the steps in order. Do not change game design. Do not edit any repository file. If a step fails, STOP and report the exact error text (copy it, do not summarise). Never print or share passwords or tokens. Do not claim success unless the zip exists and the game started.

## Facts that are always true
- Repository: https://github.com/121Nanami/BoldInBeauty-Project (private; Git is already logged in on this PC). Branch: `claude/optimistic-tesla-g2pv4m`. Never push. Never touch `main`.
- The game is an Unreal Engine 5.8 C++ project at `Demo\BoldInBeautyDemo\BoldInBeautyDemo.uproject`.
- The file `Narrator:Diamon.md` (colon in the name, which Windows cannot create) has been REMOVED from this branch at the owner's request, so a normal clone works. If you ever see `invalid path` for any file name, STOP and report it; do not change Git safety settings.
- The maps, Blueprints and textures are stored with Git LFS. GitHub's "Download ZIP" does not include them (130-byte stubs) and the build fails. Always use git plus `git lfs pull`.
- Use a short folder name that does not exist yet. Never overwrite an existing folder.
- Build in Development configuration, never Shipping (ability effects are debug-draw shapes that Shipping removes). The packaging script already does this.
- Unreal 5.8 may need the Visual Studio 2022 toolchain (MSVC v143). If the build says the compiler or toolchain is missing, install these in the Visual Studio Installer: "Game development with C++", "MSVC v143 - VS 2022 C++ x64/x86 build tools", and a Windows 10/11 SDK.
- Needs about 60 GB free disk and 20 to 60 minutes for the first build.
- The packaging script has never run on Windows. A failure is useful: report it exactly. Do not guess fixes inside the repository.

## Step 1: get the project
Open a terminal (File Explorer address bar: type `cmd`, press Enter). Run ONE LINE AT A TIME, in this exact order:
```
git --version
git lfs version
git lfs install
git clone --branch claude/optimistic-tesla-g2pv4m https://github.com/121Nanami/BoldInBeauty-Project.git C:\BIB6
cd C:\BIB6
git lfs pull
```
(Use a folder name that does not exist yet; if `C:\BIB6` exists pick another such as `C:\BIB7`. Old failed folders can be deleted with `rmdir /s /q C:\BIB2`.) Do not use "Download ZIP".

Verify before continuing:
1. `Demo\BoldInBeautyDemo\Content\BIB\Maps\L_Hub.umap` is much larger than 130 bytes. If it is about 130 bytes, run `git lfs pull` again and recheck.
2. `Demo\BoldInBeautyDemo\Tools\Package-Windows.ps1` exists.

## Step 2: check the tools
- Unreal Engine 5.8 installed (usually `C:\Program Files\Epic Games\UE_5.8`; `Engine\Build\BatchFiles\RunUAT.bat` must exist inside it).
- Visual Studio with the "Game development with C++" workload (see the toolchain note above).

## Step 3: build
Open a terminal in `Demo\BoldInBeautyDemo` (inside the clone) and run:
```
powershell -NoProfile -ExecutionPolicy Bypass -File Tools\Package-Windows.ps1
```
Add `-Engine "D:\path\to\UE_5.8"` if Unreal is not in the default folder. Leave it running. The log is `Build\Windows\PackageWindows.log`. On success it prints the zip path and a SHA-256; the zip is in `Demo\BoldInBeautyDemo\Build\Windows\`.

## Step 4: if it fails
Open `Build\Windows\PackageWindows.log`. Search for `Error:`, `error C`, `LNK`, `fatal`. Report: the first 5 distinct error lines with file name and line, the last 40 lines of the log, and which stage failed (compile, cook, package, smoke test, zip). Do not edit repository files.

## Step 5: check it runs
Unzip the zip into a new folder (for example `C:\BIBtest`); do not run it from inside the zip. Double-click `BoldInBeauty-Tester.bat`. If Windows says "Windows protected your PC", click "More info", then "Run anyway". Choose "Play as guest". Confirm the hub appears. Open Lobby, then the "Tester panel" tile, then "Currencies", and change an amount. Report pass or fail, with a screenshot of the hub and of the Tester panel if possible.

## Report back with
- zip file name, size and SHA-256
- results of Step 3 and Step 5
- the exact text of any errors, and what you did about them
