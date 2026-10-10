# Dream Air LH1 0.1.0 — experimental

This is a compiled fork of sboys3 CustomHeadsetOpenVR **1.3.0**, with an added Dream Air Lighthouse pose guard. It targets your Dream Air firmware **1.0.3**, two **Lighthouse 1.0** stations with a sync cable, and the existing **A/b** setup. Hardware support comes from Pimax/Valve's existing tracking stack; this project does not replace its optical decoder or change firmware.

## Install

1. Close SteamVR and the CustomHeadset GUI. Keep the station sync cable connected, with modes A/b.
2. Right-click **Install-Play.cmd → Run as administrator** if you use Pimax Play or are unsure. This installs the fork and an unmodified sboys3 private-only helper using the upstream documented arrangement. If you explicitly run **Pimax EVO OpenPort**, use **Install-OpenPort.cmd** instead and leave OpenPort enabled in EVO.
3. Start Pimax and SteamVR normally. The original GUI is included in `CustomHeadsetGUI`; your IPD, visual settings, and custom distortion profiles stay in place. Close the GUI before using the profile scripts to avoid competing settings writes.
4. Run **Collect-Diagnostics.cmd**. In `%APPDATA%\CustomHeadset\dreamair-tracking.json`, `build` must be `1.3.0-dreamair-lh1.1-experimental`, `deviceIndex` must be your HMD index (normally 0), and `samples` must increase. If the file is absent or `deviceIndex` is 4294967295, the fork is not processing the headset; do not assume it is active.

The installer refuses to replace a running driver. It does not close games, change firmware, modify Lighthouse calibration databases, or restart services. Backups are kept in this folder's `Backups` directory. Keep this whole folder until you no longer need rollback.

## Profiles

- **Profile-Guard.cmd**: default. Honest source tracking validity, corrupt-pose validation, conservative jump rejection, and three-sample confirmation after discontinuities. No smoothing delay. Start here for Gorilla Tag.
- **Profile-Jitter.cmd**: Guard plus adaptive position/velocity filtering. Try only after confirming station tracking is reliable; it can reduce small stationary jitter and adds some delay.
- **Profile-Smooth.cmd**: Jitter plus orientation/angular-velocity filtering. Optional and more likely to feel delayed in fast games.
- **Profile-Baseline.cmd**: filter disabled and forceTracking off. Useful for comparing the underlying driver behavior. This still uses the fork's upstream 1.3.0 code; it is not the exact old beta driver.

Profiles reload live. All disable global `forceTracking` and `forceTrackingHeadsetOnly`. These options previously made invalid positions look valid and can turn a tracking dropout into apparent floating/drift. **Keep both flags off, including in the GUI.** The Play private helper can also hook poses; its force-tracking behavior is outside this fork's guard. Re-enabling either flag can invalidate source-status diagnostics and defeat honest tracking-loss reporting. `forceTrackingRequested` in the dedicated diagnostic file should be false. Other devices retain upstream semantics.

## What this can and cannot improve

It can reject NaN/Inf, unusable rotations, excessive predictor velocities, and abrupt implausible position/rotation jumps, and avoid filtering across loss or coordinate changes. Optional smoothing can reduce small jitter. Rejected data stays **invalid**, so SteamVR may show a gray screen instead of moving the view to a corrupt position. This is deliberate; the build does not claim that freezing or guessing is recovered optical tracking.

It cannot restore missing laser sweeps, fix damaged sensors, or prove whether a plausible slow drift is real motion. Three consistent samples in a new location can represent either a genuine coordinate correction or consistently wrong upstream tracking. Gray-screen frequency, tracking quality, latency, and all headset features still require testing on your headset. This build was compiled and tested with synthetic poses and read-only OpenVR recording; it has not yet been loaded into your active SteamVR session.

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
