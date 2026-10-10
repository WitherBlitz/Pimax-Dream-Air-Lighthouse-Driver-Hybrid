# Dream Air Lighthouse Driver Hybrid 0.3.7 — experimental

Release 0.3.7 adds the launcher distortion selector and bundles the requested custom default. The native driver remains the exact working .6 build (`1.3.0-dreamair-lh1.6-experimental`); tracking, rendering and prediction code are unchanged. The user confirmed native headset motion and Lighthouse controller tracking in .6.

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

- `build`: `1.3.0-dreamair-lh1.6-experimental`, and `deviceIndex` matching the Dream Air (normally 0). The .7 package retains this native driver version.
- `poseSource`: `PimaxRuntime`; increasing callback counts, genuine source validity and a fresh last-sample time.
- The legacy `pimaxPose.aligned` field describes the old bridge and is not a success indicator for this mode. Forced runtime validity alone is insufficient.

The recorder uses runtime output with zero prediction and the raw coordinate origin; it does not record raw optical data. A stationary capture cannot establish game stability. Action output and logs are available in the launcher and **LauncherLogs**.

## Distortion profiles

The launcher lists Dream Air compatible built-in profiles from the driver's live `info.json`, compatible custom JSON files in `%APPDATA%\CustomHeadset\Distortion`, and the bundled **Dream Air Maximum Comfort 100pct Overlap** profile. Your current valid selection is shown when the window opens. Opening the launcher never writes settings. The custom profile is first in the list and is the default for a fresh installation; reinstallation preserves an existing explicit selection.

Choose a profile and click **Apply distortion**. SteamVR can stay running: the driver watches this setting and reloads it live. The launcher closes the original CustomHeadset GUI to prevent competing writes, saves the exact previous settings in **DistortionBackups**, then changes only `dreamAir.distortionProfile`. Tracking, force flags, IPD, eye rotation, other optics, floor and universe values are preserved. Existing distortion files are never overwritten. Missing, malformed, incompatible or unsafe profile names are rejected before settings are changed.

Custom profile JSON may contain the driver's supported `//` and `/* */` comments. Curve validation rejects data that would become nonfinite or degenerate in the native sampler, including singleton color curves and repeated degree coordinates. Schema field names use their exact documented case.

The bundled custom file is byte-identical to the requested profile: SHA256 `00B17F0D11166C0796F344EB3F414FE113E898CBDA04E9C40A94C05B0FC8806E`. Its distortion and chromatic-aberration curves come from SBoys3's camera-calibrated Dream Air Default; this is generic profile data, not personal calibration. Its description calls for zero eye rotation for 100% overlap. Applying a profile preserves your eye rotation rather than changing it automatically.

For script use: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Set-Distortion.ps1 -ProfileName "Dream Air Maximum Comfort 100pct Overlap"`. Profile names are JSON filenames without the `.json` extension. The bundled file is added to the driver's distortion folder only when applying it or installing the release.

## Floor height

The launcher includes the exact nominal **6 ft 3 in = 75 in = 1.905 m** preset and custom feet/inches, centimetres or metres. SteamVR must be running. Hold the headset at the desired reference position and keep it still during the five-second delay and capture. After applying, the floor stays fixed; crouching and jumping still work.

**Floor writes currently refuse either force-tracking flag.** They cannot safely calibrate from a possibly held/invalid pose. Profile changes no longer switch those flags off for you. For floor application, temporarily turn both flags off in the original headset settings, confirm genuine tracking, apply the height, then restore your preferred setting. Readout does not change the floor. Real floor apply/restore remains unverified; only read-only measurement and simulated transaction tests have been completed.

Floor changes adjust only the standing origin's vertical offset, shared by tracked devices. Backups are saved in **FloorBackups** before committing. Floor restore is separate from driver rollback and refuses a changed universe or conflicting origin. Games using seated/raw origins or their own height controls may ignore the standing floor. Later room setup or recentering can replace the floor adjustment.

## Restore and provenance

Close SteamVR and the headset GUI before **Restore driver** or `Rollback.cmd`. It restores the exact drivers/settings saved immediately before the latest installation, replacing later settings edits. It does not change Pimax's OpenPort setting; also switch OpenPort off when returning to the stock renderer. Distortion files are preserved. Keep **Backups**, **FloorBackups**, **ProfileBackups** and **DistortionBackups** when moving to a new release. The inherited remove-before-copy sequencing of manual driver restore remains unchanged.

`Source.zip` contains corresponding public source, tests, GUI source, pinned dependency sources and original GPLv2 notices. Rebuild with `Tools\Build.ps1` and Visual Studio C++ tools (default v145; override `-Toolset v143` for an appropriate VS 2022 installation). Builds pass `SkipPostBuild=true` and do not copy to live SteamVR. Binary hashes and source revisions are in **PROVENANCE.json** and **SHA256SUMS.txt**.

The bundled GUI and Play private helper are unchanged official [sboys3 1.3.0 artifacts](https://github.com/sboys3/CustomHeadsetOpenVR/releases/tag/1.3.0). The private helper's source is unavailable; OpenPort avoids that helper. Public rendering, distortion, shaders, eye tracking, foveated rendering, buttons/proximity and visual settings are inherited. New pose/floor controls use this launcher rather than new tabs in the upstream GUI. This is an unofficial experimental fork.
