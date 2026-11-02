# Windows build of the tester demo

*Status 2026-10-05. **Prepared, not built.** Unreal Engine can only build the Windows (Win64) target on a Windows PC with Visual Studio, so no AI could produce the `.exe` from the cloud container or from the Mac. The script below does everything in one go once it runs on Windows. Nobody has run it yet: the first run is the test.*

## What you get

A zip, `BoldInBeauty-Tester-Windows-<date>.zip`, to share with testers. Inside: the game, `BoldInBeauty-Tester.bat` (the launcher) and `README-TESTERS.txt`.

- It is a **Development** build on purpose. The ability effects are debug-draw shapes, which Shipping builds remove. Do not switch it to Shipping.
- **Everyone who joins is a tester**, guest or signed in: Lobby > **Tester panel** sets every currency, operator unlocks, levels and upgrade stars, campaign progress, gear, XP, the day, summon pity and has whole-profile presets. The admin account (`admin@example.com`, on a signed-in session) keeps its unlimited wallet, every operator and every level; the panel does not reduce it.
- **Guest play works with no server at all.** Signing in or creating an account needs the hosted account server (`docs/hosting.md`); until you host one, tell testers to use **Play as guest**. Guest progress is saved on their PC only.
- To make a release (non-tester) build later, set `Config::TesterMode = false` in `Source/BoldInBeautyDemo/BIBRules/BIBConfig.h`. The panel and the "Skip tutorial" button disappear and the actions refuse to run.

## One-time setup on the Windows PC

1. **Unreal Engine 5.8** from the Epic Games Launcher (the same 5.8.x as the Mac if you can).
2. **Visual Studio 2022** (Community is fine) with the workload **Game development with C++**. Add the **Windows 10/11 SDK** and **.NET desktop development** if the installer does not already tick them.
3. Get the project: `git clone` the repository (this branch or `main` once merged) and run `git lfs pull` so the `.uasset` and `.umap` files are real files and not 130-byte pointers. Git LFS is installed with Git for Windows.
4. Optional, for sign-in: deploy the account server (`docs/hosting.md`) and note its `https://` address.

## Build

Double-click `Demo\BoldInBeautyDemo\Tools\Package-Windows.bat`, or in PowerShell:

```powershell
cd Demo\BoldInBeautyDemo
powershell -ExecutionPolicy Bypass -File Tools\Package-Windows.ps1
# with sign-in against your hosted server:
powershell -ExecutionPolicy Bypass -File Tools\Package-Windows.ps1 -ApiUrl https://your-server.example.com
# engine installed somewhere else:
powershell -ExecutionPolicy Bypass -File Tools\Package-Windows.ps1 -Engine "D:\UE_5.8"
```

The first run takes 20 to 60 minutes (compiling the engine modules the project needs, cooking 637 packages, packaging). The result is in `Demo\BoldInBeautyDemo\Build\Windows\`, and the script prints the SHA-256 of the zip. The script then starts the packaged game headless for 30 seconds and fails on a crash or a fatal error in the log; that is a smoke test, not a play-through, so **start the game once yourself** (`BoldInBeauty-Tester.bat`): play as guest, open Lobby > Tester panel, clear a level.

If it fails, send back `Build\Windows\PackageWindows.log` (search it for `Error:`).

## Sharing it

- Google Drive, itch.io (a private or password-protected page works), or a GitHub release. The zip is large (several hundred MB expected); host it where the size is allowed.
- The exe is **not code-signed**, so Windows SmartScreen shows "Windows protected your PC" the first time: testers click **More info**, then **Run anyway**. `README-TESTERS.txt` says so. Code signing needs a certificate bought by the owner and is not set up.
- Publishing the game to other people has legal and content decisions that are the owner's (names, art, the demo's placeholder portraits, store terms). This repository does not make them.

## What is and is not verified

| Item | Status |
| --- | --- |
| Rules, tester panel, tutorial skip, presets (headless unit tests, 4,000+ checks) | passes on Linux (g++/clang with Unreal-like flags) |
| Whole-campaign playthrough bot (300 runs over five play styles) | passes: every run finishes, no soft-lock |
| Unreal editor build and PIE of the tester panel | **OPEN**: needs the Mac helper session (see the report) |
| Win64 compile, cook, package, smoke test, play-through | **OPEN**: needs a Windows PC |
| Code signing, store listing, hosted account server | **OPEN**: owner decisions and logins |
| The source has no platform-specific code (no `PLATFORM_*`, no POSIX headers, no absolute paths) | checked by search, which is not a Windows compile |

The macOS app was packaged earlier from the same project; the Windows target adds only the MSVC compiler and the Windows cook.
