**This driver was created and compiled entirely by GPT 6 Astra and 6.1 Sol with additions and edits by me.**

# Pimax Dream Air Lighthouse Driver Hybrid

An unofficial fork of [sboys3/CustomHeadsetOpenVR](https://github.com/sboys3/CustomHeadsetOpenVR) for the Pimax Dream Air with Lighthouse 1.0 tracking. It combines Pimax's native headset poses with the existing custom renderer and distortion profiles, and aligns Lighthouse controllers and trackers to the headset's floor space.

The working 0.3.6 setup was confirmed on October 10, 2026: headset motion and tracking were stable, the previous custom optics were restored, and both controllers matched their physical positions. Testing used Dream Air firmware 1.0.3 and two Lighthouse 1.0 stations in A/b with a sync cable. This is experimental software; the result is from one setup rather than a guarantee for every headset or game.

Release 0.3.7 adds the launcher distortion selector and retains the exact confirmed 0.3.6 driver binary. Choose a compatible built-in or custom Dream Air profile and click **Apply distortion**. The bundled **Dream Air Maximum Comfort 100pct Overlap** profile is selected by default when no previous selection exists. Opening the launcher never changes settings; applying a profile saves a settings backup and changes only the Dream Air distortion selection. Close the original CustomHeadset GUI before applying. Other optical controls remain in **Headset settings**.

## Download and run

Download the ZIP from [Releases](https://github.com/WitherBlitz/Pimax-Dream-Air-Lighthouse-Driver-Hybrid/releases), extract the whole folder into a writable location, and open **DreamAir-LH1.exe**. Keep the launcher, scripts, driver folders and Tools together.

1. Close SteamVR and the original CustomHeadset settings GUI.
2. Keep Pimax running and enable **EVO OpenPort**.
3. In the launcher, select **EVO OpenPort** and the **Pimax** tracking profile, then click **Install driver**.
4. Start SteamVR and check headset motion and controller alignment.

Installation backs up the existing driver and settings. **Restore driver** restores that installation backup. The complete instructions and limitations are in [START-HERE](Tools/Package/START-HERE.md).

## Features

- Native Pimax FloorLevel headset tracking without recentering the room.
- Correct rotational prediction frames for head turns, with native linear velocity preserved.
- A rigid mapping of Lighthouse controller, tracker and station world poses into the native headset space.
- Existing sboys3 distortion, visual settings and custom renderer.
- A launcher distortion selector with **Dream Air Maximum Comfort 100pct Overlap** as the default on fresh installations. Existing selections are preserved.
- A Windows EXE launcher, tracking profiles, diagnostics, installation and rollback.
- A **6 ft 3 in / 1.905 m** floor preset and custom feet/inches, centimetres or metres.

Install/profile actions preserve the existing force-tracking settings. Force tracking can mark a held pose valid during tracking loss; it does not recover missing movement. Diagnostics distinguish genuine pose health from forced validity. Floor writes currently require both force-tracking settings to be disabled temporarily, and live floor apply/restore remains unverified.

## Controller alignment and OpenXR

The driver can align coordinate origins, but a rigid transform cannot correct conflicting station geometry saved separately by SteamVR and Pimax. On the confirmed setup, resolving that software layout mismatch was also necessary. The published release does not contain or apply the owner's room calibration. See [the alignment notes](Docs/DreamAir-LH1/ALIGNMENT.md) for the measured cause and repair scope.

For OpenXR games, select **SteamVR's OpenXR runtime** so the game uses this SteamVR driver. Keep Pimax running with OpenPort enabled. A native Pimax OpenXR rendering session bypasses this custom renderer and its distortion profile. Game compatibility and extended reconnect behavior have not been established across all titles.

## Build from source

Clone with submodules:

```powershell
git clone --recurse-submodules https://github.com/WitherBlitz/Pimax-Dream-Air-Lighthouse-Driver-Hybrid.git
cd Pimax-Dream-Air-Lighthouse-Driver-Hybrid
```

Install Visual Studio C++ desktop build tools and a Windows SDK, then run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\Build.ps1
```

The tested toolset is MSVC v145. An appropriate VS 2022 installation can select `-Toolset v143`. The build compiles the driver, tools and launcher and runs the native unit suites. It passes `SkipPostBuild=true` and does not install into the live SteamVR directory. Pimax software must supply a compatible PVR client for native tracking.

To package, extract the original [sboys3 1.3.0 Windows release](https://github.com/sboys3/CustomHeadsetOpenVR/releases/tag/1.3.0), then choose a fresh destination:

```powershell
python .\Tools\Package.py --upstream C:\Build\CustomHeadset-1.3.0-Windows --destination C:\Build\DreamAir-LH1-release
```

This preserves the original GUI and optional private helper binaries. Their private implementation is not part of this public source; the OpenPort configuration avoids the private helper. See [validation](Tools/Package/VALIDATION.md) for evidence and remaining limitations.

## Credits and license

Based on sboys3 CustomHeadsetOpenVR 1.3.0, upstream commit `21185da63a9e4307ae876b3702a43a43044d451c`. Original authorship, dependency notices and the [GPLv2 license](LICENCE.md) are retained. This fork is not an official Pimax or sboys3 release. The [upstream README](Docs/DreamAir-LH1/UPSTREAM-README.md) is preserved for original feature and author information.

Personal settings, tracking databases, diagnostics and restore snapshots are kept locally and are not release assets.

This driver is for a fixing of the Sboys3 driver for my basestation 1.0s, essentially being a hybrid driver between the original Pimax driver as well as the sboys3 driver. If you're having issues with basestation 1.0s or tracking jitter of some kind or tracking loss and drift, give this hybrid driver a shot.
