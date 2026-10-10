# Dream Air LH1 0.3.3 — experimental

This fork retains the public sboys3 CustomHeadsetOpenVR 1.3.0 rendering and headset features and adds an optional **Pimax HMD pose source**. It targets the Dream Air, firmware 1.0.3, with your Lighthouse 1.0 A/b sync-cable setup. Controllers keep their existing SteamVR tracking. The new mode is an experiment based on measured software behavior; complete feature parity and game stability have not been established.

## Open and install

Extract the whole ZIP into a writable folder and open **DreamAir-LH1.exe**. Keep the EXE, scripts, driver folders and Tools together. No developer tools are required. Opening the window changes no settings.

The launcher starts with **EVO OpenPort** and **Pimax** selected for your current setup. Close SteamVR and the CustomHeadset GUI, then click **Install driver**. Driver installation/restore needs administrator elevation and saves backups. Start Pimax and SteamVR normally. For a Pimax Play connection, select that backend instead; the bundled official private helper remains unchanged.

**Keep the headset still briefly during SteamVR startup.** Pimax mode waits for genuinely valid Lighthouse and Pimax poses at matching times, then requires a consistent steady alignment window. It maps Pimax's head pose into the existing SteamVR room and keeps the transform fixed for that session. After alignment, head tracking continues even if SteamVR's Lighthouse HMD callbacks stop. This does not replace the optical decoder or modify firmware/calibration files.

If alignment never completes, the original Lighthouse path stays active. Diagnostics report `pimaxPose.aligned=false`; native polling alone does not prove the new stream reached SteamVR. Restart SteamVR after changing the Pimax runtime's tracking origin/recentering or reconnecting the headset, so the room transform is rebuilt. Check controller alignment after a successful startup.

## Tracking profiles and diagnostics

- **Pimax**: new native HMD pose source; additional guard/smoothing disabled. Recommended experiment for the measured SteamVR-only failure.
- **Baseline**: original Lighthouse HMD source with the added guard disabled.
- **Guard**: original source with validation, discontinuity rejection and reacquisition, without smoothing.
- **Jitter**: Guard plus adaptive position filtering.
- **Smooth**: Jitter plus orientation filtering, with additional delay.

**Install and profile changes preserve existing `forceTracking` and `forceTrackingHeadsetOnly` values.** Your requested `forceTracking=true` can hide a tracking failure by marking a held pose valid. It does not recover missing motion. Diagnostics therefore expose genuine source validity before forced validity. Guard rejections may still produce gray screens.

Profiles reload live. Close the original headset GUI before applying them, to avoid competing settings writes. The launcher forwards the selected profile during installation. Direct `Install-OpenPort.cmd`/`Install-Play.cmd` retain the Guard default; run `Profile-Pimax.cmd` afterwards for native HMD poses, or pass `-Profile Pimax` to the installation command.

Use **Collect diagnostics** with SteamVR running. It saves logs/settings and a 30-second OpenVR CSV locally in **Diagnostics**. With SteamVR closed, it still collects available logs but cannot record live poses. Nothing is uploaded. The separate `%APPDATA%\CustomHeadset\dreamair-tracking.json` should show:

- `build`: `1.3.0-dreamair-lh1.3-experimental`, and `deviceIndex` matching the Dream Air (normally 0).
- `poseSource`: `Pimax`; `pimaxPose.aligned`: true after steady alignment.
- `pimaxPose.samples` increasing, `sourceValid`: true, and a fresh `secondsSinceLastSample`.
- Original Lighthouse counts and validity separately from native/output counts. Forced runtime validity alone is insufficient.

The recorder uses runtime output with zero prediction and the raw coordinate origin; it does not record raw optical data. A stationary capture cannot establish game stability. Action output and logs are available in the launcher and **LauncherLogs**.

## Floor height

The launcher includes the exact nominal **6 ft 3 in = 75 in = 1.905 m** preset and custom feet/inches, centimetres or metres. SteamVR must be running. Hold the headset at the desired reference position and keep it still during the five-second delay and capture. After applying, the floor stays fixed; crouching and jumping still work.

**Floor writes currently refuse either force-tracking flag.** They cannot safely calibrate from a possibly held/invalid pose. Profile changes no longer switch those flags off for you. For floor application, temporarily turn both flags off in the original headset settings, confirm genuine tracking, apply the height, then restore your preferred setting. Readout does not change the floor. Real floor apply/restore remains unverified; only read-only measurement and simulated transaction tests have been completed.

Floor changes adjust only the standing origin's vertical offset, shared by tracked devices. Backups are saved in **FloorBackups** before committing. Floor restore is separate from driver rollback and refuses a changed universe or conflicting origin. Games using seated/raw origins or their own height controls may ignore the standing floor. Later room setup or recentering can replace the floor adjustment.

## Restore and provenance

Close SteamVR and the headset GUI before **Restore driver** or `Rollback.cmd`. It restores the exact drivers/settings saved immediately before the latest installation, replacing later settings edits. Distortion files are preserved. Keep **Backups**, **FloorBackups** and **ProfileBackups** when moving to a new release. The inherited remove-before-copy sequencing of manual driver restore remains unchanged.

`Source.zip` contains corresponding public source, tests, GUI source, pinned dependency sources and original GPLv2 notices. Rebuild with `Tools\Build.ps1` and Visual Studio C++ tools (default v145; override `-Toolset v143` for an appropriate VS 2022 installation). Builds pass `SkipPostBuild=true` and do not copy to live SteamVR. Binary hashes and source revisions are in **PROVENANCE.json** and **SHA256SUMS.txt**.

The bundled GUI and Play private helper are unchanged official [sboys3 1.3.0 artifacts](https://github.com/sboys3/CustomHeadsetOpenVR/releases/tag/1.3.0). The private helper's source is unavailable; OpenPort avoids that helper. Public rendering, distortion, shaders, eye tracking, foveated rendering, buttons/proximity and visual settings are inherited. New pose/floor controls use this launcher rather than new tabs in the upstream GUI. This is an unofficial experimental fork.
