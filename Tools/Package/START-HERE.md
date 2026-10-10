# Dream Air LH1 0.3.6 — experimental

This fork retains the public sboys3 CustomHeadsetOpenVR 1.3.0 rendering and headset features and adds an optional **Pimax HMD pose source**. It targets the Dream Air, firmware 1.0.3, with your Lighthouse 1.0 A/b sync-cable setup. Controllers keep their existing SteamVR tracking. The new mode is an experiment based on measured software behavior; complete feature parity and game stability have not been established.

## Open and install

Extract the whole ZIP into a writable folder and open **DreamAir-LH1.exe**. Keep the EXE, scripts, driver folders and Tools together. No developer tools are required. Opening the window changes no settings.

The launcher starts with **EVO OpenPort** and **Pimax** selected for your current setup. Close SteamVR and the CustomHeadset GUI, enable OpenPort in Pimax, then click **Install driver**. Driver installation/restore saves backups. Start Pimax and SteamVR normally. The Pimax pose profile requires OpenPort; Play/private-only mode cannot supply this custom renderer.

Pimax mode creates its own FloorLevel tracking session and uses Pimax's reported room universe, without recentering. It suppresses the duplicate Lighthouse Dream Air HMD while retaining Lighthouse controllers. Existing Dream Air distortion, IPD, eye rotation and other optical settings are preserved. This does not replace the optical decoder or modify firmware/calibration files.

This profile does not use the earlier timed-alignment bridge or require valid Lighthouse HMD callbacks. Reconnecting reapplies FloorLevel to the driver session. It reads the active Pimax room's standing yaw to map rotational velocity into the correct prediction frame. Linear velocity already uses FloorLevel coordinates. Check controller alignment and head movement after startup.

The .6 build also aligns Lighthouse controllers, trackers and station models to the native headset's existing FloorLevel space. It matches the primary station by serial number between runtimes, composes a rigid world transform, and preserves controller pose/prediction/input fields. It never recenters the headset or rewrites room calibration. Static station callbacks are replayed through their owning Lighthouse host after alignment. A station calibration mismatch between runtimes can leave a secondary station model offset; controller alignment uses the primary station rather than averaging inconsistent anchors.

Installation enables SteamVR's `activateMultipleDrivers` for the Pimax profile so Lighthouse remains loaded alongside the custom headset. Live testing confirmed valid native head poses after enabling this setting; the user confirmed the old optics were restored and tracking remained good.

## Tracking profiles and diagnostics

- **Pimax**: Pimax runtime HMD poses and custom rendering; additional guard/smoothing disabled.
- **Baseline**: original Lighthouse HMD source with the added guard disabled.
- **Guard**: original source with validation, discontinuity rejection and reacquisition, without smoothing.
- **Jitter**: Guard plus adaptive position filtering.
- **Smooth**: Jitter plus orientation filtering, with additional delay.

**Install and profile changes preserve existing `forceTracking` and `forceTrackingHeadsetOnly` values.** Your requested `forceTracking=true` can hide a tracking failure by marking a held pose valid. It does not recover missing motion. Diagnostics therefore expose genuine source validity before forced validity. Guard rejections may still produce gray screens.

Changing into or out of Pimax mode requires SteamVR to be closed. Other filter profiles reload live. Close the original headset GUI before applying profiles, to avoid competing settings writes. The launcher forwards the selected profile during installation. Direct install commands retain the Guard default; select Pimax with the OpenPort backend to use native poses.

Use **Collect diagnostics** with SteamVR running. It saves logs/settings and a 30-second OpenVR CSV locally in **Diagnostics**. With SteamVR closed, it still collects available logs but cannot record live poses. Nothing is uploaded. The separate `%APPDATA%\CustomHeadset\dreamair-tracking.json` should show:

- `build`: `1.3.0-dreamair-lh1.5-experimental`, and `deviceIndex` matching the Dream Air (normally 0).
- `poseSource`: `PimaxRuntime`; increasing callback counts, genuine source validity and a fresh last-sample time.
- The legacy `pimaxPose.aligned` field describes the old bridge and is not a success indicator for this mode. Forced runtime validity alone is insufficient.

The recorder uses runtime output with zero prediction and the raw coordinate origin; it does not record raw optical data. A stationary capture cannot establish game stability. Action output and logs are available in the launcher and **LauncherLogs**.

## Floor height

The launcher includes the exact nominal **6 ft 3 in = 75 in = 1.905 m** preset and custom feet/inches, centimetres or metres. SteamVR must be running. Hold the headset at the desired reference position and keep it still during the five-second delay and capture. After applying, the floor stays fixed; crouching and jumping still work.

**Floor writes currently refuse either force-tracking flag.** They cannot safely calibrate from a possibly held/invalid pose. Profile changes no longer switch those flags off for you. For floor application, temporarily turn both flags off in the original headset settings, confirm genuine tracking, apply the height, then restore your preferred setting. Readout does not change the floor. Real floor apply/restore remains unverified; only read-only measurement and simulated transaction tests have been completed.

Floor changes adjust only the standing origin's vertical offset, shared by tracked devices. Backups are saved in **FloorBackups** before committing. Floor restore is separate from driver rollback and refuses a changed universe or conflicting origin. Games using seated/raw origins or their own height controls may ignore the standing floor. Later room setup or recentering can replace the floor adjustment.

## Restore and provenance

Close SteamVR and the headset GUI before **Restore driver** or `Rollback.cmd`. It restores the exact drivers/settings saved immediately before the latest installation, replacing later settings edits. It does not change Pimax's OpenPort setting; also switch OpenPort off when returning to the stock renderer. Distortion files are preserved. Keep **Backups**, **FloorBackups** and **ProfileBackups** when moving to a new release. The inherited remove-before-copy sequencing of manual driver restore remains unchanged.

`Source.zip` contains corresponding public source, tests, GUI source, pinned dependency sources and original GPLv2 notices. Rebuild with `Tools\Build.ps1` and Visual Studio C++ tools (default v145; override `-Toolset v143` for an appropriate VS 2022 installation). Builds pass `SkipPostBuild=true` and do not copy to live SteamVR. Binary hashes and source revisions are in **PROVENANCE.json** and **SHA256SUMS.txt**.

The bundled GUI and Play private helper are unchanged official [sboys3 1.3.0 artifacts](https://github.com/sboys3/CustomHeadsetOpenVR/releases/tag/1.3.0). The private helper's source is unavailable; OpenPort avoids that helper. Public rendering, distortion, shaders, eye tracking, foveated rendering, buttons/proximity and visual settings are inherited. New pose/floor controls use this launcher rather than new tabs in the upstream GUI. This is an unofficial experimental fork.
