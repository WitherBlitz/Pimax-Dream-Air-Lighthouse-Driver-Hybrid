# Dream Air LH1 0.3.2 — experimental

This is a compiled fork of sboys3 CustomHeadsetOpenVR **1.3.0**, with an added Dream Air Lighthouse pose guard. It targets your Dream Air firmware **1.0.3**, two **Lighthouse 1.0** stations with a sync cable, and the existing **A/b** setup. Hardware support comes from Pimax/Valve's existing tracking stack; this project does not replace its optical decoder or change firmware.

**0.3.2 tracking correction:** the previous guard could reject valid chronological callbacks when their relative prediction offsets changed or repeated, causing extra gray screens. Filter history now uses callback arrival time and keeps each accepted pose's current predictor fields. Baseline diagnostics also keep updating when the guard is disabled. The new native regressions pass; follow the validation report for the boundary between measured old-driver behavior and testing the corrected driver. [OpenVR describes the prediction offset here](https://github.com/ValveSoftware/openvr/blob/master/docs/Driver_API_Documentation.md#poses).

**0.3.1 installer fix:** 0.3.0 could fail while backing up resources when extracted under a long folder path. Directory backup, staging and restore now use Windows' long-path copy support, and temporary JSON metadata uses extended-length file paths. A backup copy failure still stops installation before replacing the existing driver. If you encountered that error, extract this release and retry **Install driver** with SteamVR/headset settings closed.

## Open the release

Extract the **whole ZIP** into a writable folder, then double-click **DreamAir-LH1.exe**. Keep the EXE alongside the supplied scripts, driver folders and Tools; copying only the EXE will leave its actions unavailable. This Windows x64 launcher uses the .NET Framework included with Windows 10/11. It needs no developer tools.

The window offers installation/backend selection, headset settings, tracking profiles, the 6 ft 3 in preset, custom heights, current-height readout, floor/driver restore, and diagnostics. Its result area shows successful output and errors; action logs are saved in **LauncherLogs**. Driver installation and driver rollback request Windows administrator elevation; normal floor/profile/diagnostic actions do not. Opening the window performs no driver or floor changes.

Use **Pimax Play → Install driver** unless you explicitly use EVO OpenPort. Close SteamVR and headset settings before driver install/restore. Keep SteamVR running for floor controls, hold the headset still through the five-second delay, and leave forced tracking disabled. Close headset settings before applying a tracking profile. The existing CMD/PowerShell controls below remain available.

If upgrading from an older extracted release, keep its **Backups** and **FloorBackups** folders or copy them into the new release before using restore. The launcher opens the original headset GUI for IPD/visual settings; its own controls add the tracking and floor tools. The corrected DLL still requires hardware validation at packaging time; real floor apply/restore remains unverified.

## Install

1. Close SteamVR and the CustomHeadset GUI. Keep the station sync cable connected, with modes A/b.
2. Right-click **Install-Play.cmd → Run as administrator** if you use Pimax Play or are unsure. This installs the fork and an unmodified sboys3 private-only helper using the upstream documented arrangement. If you explicitly run **Pimax EVO OpenPort**, use **Install-OpenPort.cmd** instead and leave OpenPort enabled in EVO.
3. Start Pimax and SteamVR normally. The original GUI is included in `CustomHeadsetGUI`; your IPD, visual settings, and custom distortion profiles stay in place. Close the GUI before using the profile scripts to avoid competing settings writes.
4. Run **Collect-Diagnostics.cmd** with SteamVR running. In `%APPDATA%\CustomHeadset\dreamair-tracking.json`, `build` must be `1.3.0-dreamair-lh1.2-experimental`, `deviceIndex` must be your HMD index (normally 0), and `samples` must increase even in Baseline. If the file is absent or `deviceIndex` is 4294967295, the fork is not processing the headset; do not assume it is active. With SteamVR closed, the recorder cannot capture live poses; the script still saves available logs before reporting that capture was unavailable.

The installer refuses to replace a running driver. It does not close games, change firmware, modify Lighthouse calibration databases, or restart services. Backups are kept in this folder's `Backups` directory. Keep this whole folder until you no longer need rollback.

## Profiles

- **Profile-Guard.cmd**: default. Honest source tracking validity, corrupt-pose validation, conservative jump rejection, and three-sample confirmation after discontinuities. No smoothing delay. Start here for Gorilla Tag.
- **Profile-Jitter.cmd**: Guard plus adaptive position/velocity filtering. Try only after confirming station tracking is reliable; it can reduce small stationary jitter and adds some delay.
- **Profile-Smooth.cmd**: Jitter plus orientation/angular-velocity filtering. Optional and more likely to feel delayed in fast games.
- **Profile-Baseline.cmd**: filter disabled and forceTracking off. Useful for comparing the underlying driver behavior. This still uses the fork's upstream 1.3.0 code; it is not the exact old beta driver.

Profiles reload live. All disable global `forceTracking` and `forceTrackingHeadsetOnly`. These options previously made invalid positions look valid and can turn a tracking dropout into apparent floating/drift. **Keep both flags off, including in the GUI.** The Play private helper can also hook poses; its force-tracking behavior is outside this fork's guard. Re-enabling either flag can invalidate source-status diagnostics and defeat honest tracking-loss reporting. `forceTrackingRequested` in the dedicated diagnostic file should be false. Other devices retain upstream semantics.

## Floor height: 6 feet 3 inches or custom

Keep SteamVR running. Hold or wear the headset at the position you want to use as the height reference, with reliable tracking. Close the CustomHeadset GUI and use **Profile-Guard.cmd** first if either force-tracking option is enabled. Floor writes refuse those flags because they can conceal invalid tracking.

- **Floor-6ft3.cmd** sets SteamVR's standing floor **6 ft 3 in = 75 in = 1.905 m** below the sampled headset position. It gives you five seconds to get into position, then requires a short steady capture before applying.
- **Floor-Custom.cmd** prompts for feet/inches, centimetres, or metres. Pressing Enter at the prompts uses 6 ft 3 in. Fractional inches are supported. Heights from 0 to 10 m are accepted.
- **Floor-Read.cmd** reports your current tracked headset height without changing calibration.
- **Floor-Restore.cmd** undoes the latest floor adjustment made by these tools.

The floor **stays fixed after you apply it**. Crouching, jumping and normal movement continue to change your height above that floor. This is the requested distance from the headset's tracked origin, not a measurement of your body's height. The preset conversion is exact; the resulting calibration has floating-point and tracking measurement uncertainty.

This uses Valve's chaperone API to change only the standing origin's vertical offset. Headset, controllers and trackers share the change, preserving their alignment. Standing-origin rotation, horizontal placement, seated origin, play-area size and collision geometry are not edited. SteamVR saves the calibration, but later room setup, recentering or other offset tools can replace it. Games using a seated/raw origin or their own height settings may ignore it. Avoid running room-setup/playspace-adjustment tools concurrently.

Each change saves an origin backup and full calibration export in **FloorBackups** before committing. Restore changes only the standing origin and refuses a different headset/universe or a floor origin changed by another tool since application. The installer **Rollback.cmd** restores driver/settings; floor restoration is separate. Keep FloorBackups when moving to a newer package. No administrator rights are needed for the floor controls.

For direct commands, run from this package directory:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Floor-Height.ps1 -Feet 6 -Inches 3
powershell -NoProfile -ExecutionPolicy Bypass -File .\Floor-Height.ps1 -Centimeters 180
powershell -NoProfile -ExecutionPolicy Bypass -File .\Floor-Height.ps1 -Meters 1.8
powershell -NoProfile -ExecutionPolicy Bypass -File .\Floor-Height.ps1 -Restore -BackupId YOUR-BACKUP-ID
```

Custom heights use a decimal point. Floor apply/restore was tested against a simulated runtime and real backup files, with only read-only measurement on your running SteamVR session. Live floor mutation remains unverified.

## What this can and cannot improve

It can reject NaN/Inf, unusable rotations, excessive predictor velocities, and abrupt implausible position/rotation jumps, and avoid filtering across loss or coordinate changes. Optional smoothing can reduce small jitter. Rejected data stays **invalid**, so SteamVR may show a gray screen instead of moving the view to a corrupt position. This is deliberate; the build does not claim that freezing or guessing is recovered optical tracking.

It cannot restore missing laser sweeps, fix damaged sensors, or prove whether a plausible slow drift is real motion. Three consistent samples in a new location can represent either a genuine coordinate correction or consistently wrong upstream tracking. The previous 0.3.1 DLL was measured on your headset: Baseline had 0/1914 invalid samples, while a Guard repeat had 168/1272 despite no increase in the source-invalid count. This release corrects the timestamp bug behind those added rejections. Gray-screen frequency, tracking quality, latency and all headset features with the corrected DLL still require testing.

Your logs include `Base 3858A002 axis 0 appears to have failed`, HMD out-of-bounds resets, bootstrap failures, and optical-sample loss. The warning is not proof of broken hardware, but repeated restarts working intermittently do not exclude optical/sync/firmware/hardware causes. The sync modes were independently confirmed as A/B through OpenVR. Avoid speculative firmware downgrades or optical calibration rewrites.

## Compare and capture

Start with Guard, then compare Baseline under the same headset position and lighting. For jitter, place the headset still for a 30-second capture. For jumps/drift, reproduce the issue while recording. Save a capture for each profile; snapshots while standing still cannot establish game stability. `Tools\TrackingRecorder.exe` records the HMD, controllers, and visible stations through official OpenVR, with zero prediction. Its “raw” origin is a coordinate-system label, **not raw optical data**. With forced tracking enabled in an older driver, runtime valid flags can conceal source failure.

`Collect-Diagnostics.cmd` saves CSV, settings, latest guard counters, and relevant SteamVR logs in a local `Diagnostics` folder. Nothing is uploaded. Review logs before sharing them. If failures persist, compare each station alone in HTC's single-station setup (disconnect sync cable, turn the other station off, remaining station in A) while keeping the headset in clear view. That comparison is a diagnostic exercise, not a room-scale playing setup. Consistent failure from one station is useful evidence for Pimax/HTC support.

## Roll back

Close SteamVR and the GUI, then right-click **Rollback.cmd → Run as administrator**. It restores the exact driver files and settings saved immediately before the most recent installation. It also restores a pre-existing private helper if one was present. Later settings edits will be replaced by that saved version. Distortion files are never replaced.

## Source, rebuild, and provenance

`Source.zip` contains this fork's complete public source, tests, GUI source, notices, and the exact checked-out dependency sources. Extract and run `Tools\Build.ps1` with Visual Studio C++ tools; default toolset is v145 (VS 2026), override `-Toolset v143` for a suitable VS 2022 installation. `SkipPostBuild=true` is always passed so builds do not copy to live SteamVR. The included upstream GUI binary and private helper are unchanged official release artifacts; private helper source is not public and is not recreated here. OpenPort avoids the need for that helper.

Upstream: https://github.com/sboys3/CustomHeadsetOpenVR/releases/tag/1.3.0

Public source uses GPLv2. Original notices and license are included. This is an unofficial experimental fork, with no claim of endorsement or hardware-tested feature parity. It inherits all public rendering, distortion, shaders, eye tracking, foveated rendering, buttons/proximity, and settings code; the added tracking profiles are controlled by the supplied scripts rather than new tabs in the upstream GUI. Existing unrelated custom 3DoF/controller simulation modifications in an older local binary are not part of sboys3's public 1.3.0 release.

HTC cable modes and single-station guidance: https://developer.vive.com/documents/720/Vive_User_Guide.pdf

Dream Air 1.0 compatibility: https://store.pimax.com/blogs/blogs/where-to-buy-base-stations-in-2026

Pimax tracking diagnostics: https://support.pimax.com/en/support/solutions/articles/60000812310-tracking-issue-related
