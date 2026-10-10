# Validation — Dream Air LH1 0.3.3 experimental

Built on 2026-10-10 from sboys3 public 1.3.0, upstream commit `21185da63a9e4307ae876b3702a43a43044d451c`, with pinned submodules. Exact source revision and binary hashes are in PROVENANCE.json/SHA256SUMS.txt; Source.zip includes corresponding public source.

## Evidence for the new source

The user reports stable translation and rotation in the Pimax lobby but severe jitter/loss in SteamVR. The restored station setup remains A/b with a sync cable. `forceTracking=true` eliminates gray screens but can make an invalid or frozen pose appear valid.

A simultaneous 30-second read-only comparison returned **1,916/1,916 Pimax poses with both position and orientation flags**, using Pimax tracking style 2 (Lighthouse). Pimax position/orientation changed while SteamVR's forced-valid HMD output stayed frozen; the original source callback count did not advance. This demonstrates separate software pose behavior in that session. It does not prove every earlier failure has the same cause or establish controller behavior.

The installed Valve receiver console identified both A/B stations, received hits on all 28 configured sensors, reported roughly 1,000 Hz IMU and ongoing optical packets, and decoded SensorType 9. The headset device configuration matched SteamVR's cached configuration field-for-field. These were read-only checks; no firmware, optical geometry, calibration database or station changes were made by this update. Investigation now focuses on the software path; no hardware defect is asserted.

## Completed software checks

- New bridge regressions first failed against a scaffold, then passed. Review timing regressions reproduced mismatched represented timestamps, omitted submission latency and stale native acceptance (15/18 before correction). Steady-window/motion regressions also failed before implementation. Final aggregate: **22/22 native pose, 41/41 existing pose, 50/50 floor, 11/11 launcher, 3/3 isolated elevation-host and 5/5 profile cases**.
- Native coverage includes noncommuting rotations and rotated head offsets, matching-pose timing, delayed submissions, genuine validity before force flags, finite invalid fallback, continuity after Lighthouse callbacks stop, consistent low-motion alignment, source changes, lifecycle and controller exclusion. Both native and forwarded Lighthouse offsets are rebased through serialized submission.
- Independent read-only review found timing and test-coverage defects, reproduced with focused regressions and corrected. The review does not judge live improvement or PVR coordinate stability across recenter/reconnect; restart SteamVR to rebuild alignment after such changes.
- **7/7 installer cases** passed with actual driver payloads in isolated marked sandboxes, including selected Pimax profile, force preservation, settings/distortion preservation, malformed input rejection and byte-exact rollback. Profile preservation also passed **5/5 in Windows PowerShell 5.1**. Installer and long-path shell results are recorded in the verification ledger.
- Builds use Release x64, MSVC v145 and `SkipPostBuild=true`; inherited upstream conversion warnings remain. The bundled official GUI/private helper are unchanged. Source/package checks are completed before delivery.

## Validation boundaries

At packaging time, **the new .3 native-source DLL still requires live installation and runtime testing**. A subsequent live-test report is separate from this sealed package. Unit/sandbox tests and the API comparison do not establish Gorilla Tag stability, latency, controller alignment, complete feature parity or recovery across a Pimax tracking-origin change.

The earlier .2 driver was installed and tested; its original Lighthouse path still showed invalid/frozen callbacks in the resumed session. The .2 guard correction remains included: chronological history uses callback arrival, while valid predictor fields retain their relative offsets. Baseline statistics continue updating.

Floor apply/restore is unchanged and remains unverified against the real chaperone service. Its **50/50** tests use simulated runtime behavior and real durable backup files; read-only height measurement worked. Floor writes refuse both force flags. Profiles now preserve those flags as requested, so forced-valid runtime output must not be presented as genuine source health.

Inherited upstream shutdown/API/hook cleanup behavior and private-helper internals remain outside this change. The optional mode changes only the Dream Air HMD pose route. Keep the installer backup for rollback.
