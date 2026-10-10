# Validation — Dream Air LH1 0.1.0 experimental

Built locally on 2026-10-09 from sboys3 CustomHeadsetOpenVR 1.3.0, commit `21185da63a9e4307ae876b3702a43a43044d451c`, with its pinned submodules. The public source and build scripts are in `Source.zip`; exact binary hashes and dependency revisions are in `PROVENANCE.json` and `SHA256SUMS.txt`.

## Completed checks

- Release x64 driver compiled with Visual Studio 2026 / MSVC v145. Builds use `SkipPostBuild=true`; the live SteamVR driver was not replaced. Existing upstream narrowing/conversion warnings remain.
- **35/35 native tests passed**: source validity, finite values and quaternion normalization, jump rejection, prediction bounds, recovery, coordinate changes after loss/gaps, timestamp handling, adaptive filters, device selection, activation/deactivation, settings reload, forced-tracking precedence within the public module, and concurrent callbacks.
- **6/6 installer cases passed** in both PowerShell 7 and Windows PowerShell 5.1: settings/distortion preservation, profiles, exact rollback of existing settings and both drivers, clean OpenPort installation/rollback, malformed JSON rejection, and path containment. Tests ran against isolated marked sandbox directories.
- Independent code review identified recovery and headset-selection defects; regression tests reproduced them before correction. The helper's cross-module limitations were documented rather than assumed away.
- Read-only OpenVR recording identified the Dream Air, both controllers, and two Lighthouse stations with A/B modes. A brief stationary HMD capture did not reproduce the reported intermittent failures. Recorder argument validation was checked.
- The packaged GUI and Play private helper are unchanged official 1.3.0 release binaries. The package includes corresponding public source and original notices; private helper source is unavailable upstream.

## Still requiring headset testing

The fork has **not been loaded into the live SteamVR session**. Startup, inherited headset features, game compatibility, tracking improvement, gray-screen frequency, and motion latency are unverified on hardware. Passing synthetic tests does not establish those outcomes.

This build uses the existing Pimax/Valve Lighthouse decoder. It adds pose validation and recovery handling; it cannot recover missing laser sweeps or distinguish every plausible slow drift from real movement. Rejected data is invalid and may cause a gray screen. Optional filtering adds delay. Begin with the Guard profile.

Existing logs contain optical-sample loss, bootstrap failures, HMD out-of-bounds resets, and `Base 3858A002 axis 0 appears to have failed`. These warrant investigation but do not establish a hardware defect. The previous installed driver had `forceTracking=true`, which can conceal source loss. All supplied profiles disable both force-tracking flags; leave them disabled because the separate Play helper can also intercept poses.

Inherited upstream shutdown behavior and unavailable private-helper internals remain outside this change. Keep the installer backup and use `Rollback.cmd` if testing reveals a regression.
