# Verification ledger

Execution: inline in a fresh clone; no live runtime/config writes. Approval prompts are preauthorized in user AGENTS.md. Upstream baseline commit 21185da63a9e4307ae876b3702a43a43044d451c.

- Pose core: 19 missing-behavior failures observed against passthrough; implemented; 24/24 native tests passed.
- Integration/metadata: added actual-index, force-precedence, hot reload, concurrency, quaternion wrap tests. Rotation-only diagnostics regression observed (30/31); fixed; 31/31 passed. Release x64 full driver and recorder built with v145, SkipPostBuild=true. Upstream hard-coded Pimax SDK include failed; changed to pinned existing header.
- Installer: first sandbox run exposed PowerShell null-string marshalling in File.Replace. Transaction automatically restored original files. Changed to unique temporary backup path; 6/6 sandbox install/profile/rollback/malformed-state/containment cases passed, with exact byte hashes for settings restoration.
- Evidence: source firmware/setup supplied by user. Read-only OpenVR recording confirms base modes A/B; brief capture did not reproduce HMD discontinuity. ForceTracking=true can conceal source losses. Current log includes HMD out-of-bounds and Base 3858A002 axis 0 warning.
- Fresh reviewer: identified three Important findings, no Critical findings. Added four regressions for coordinate changes after loss/gap and inactive/unrelated shim registration; all four failed (31/35). Fixed recovery ordering and activation eligibility. Removed unsupported cross-module forced-tracking guarantee; exposed forceTrackingRequested and documented both shared flags must stay false.
- Final: Ruling: keep the published private helper unchanged and require its force flags off — the public fork cannot guarantee another binary's pose behavior; cost if wrong: invalid poses can look valid again if a tool re-enables those flags. Defaults/presets disable them and diagnostics disclose the setting.
- Final: Ruling: existing upstream detached-thread/empty-Cleanup behavior remains inherited — no new ownership or shutdown threads added; cost if wrong: upstream shutdown/reload defects may still occur.
- Final: Ruling: hardware/game runtime validation remains pending — active VR was not interrupted to load the experimental build; cost if wrong: startup, feature compatibility, latency, and tracking quality may differ on actual hardware.
- No deferred minor findings. Final build/test/package verification results are recorded in the delivered validation report.

## Floor-height update

- User requested 6 ft 3 in below the current headset and custom heights. Ruling: use a fixed standing-origin floor applied on demand — preserves crouching and common device alignment — cost if wrong: a user who wanted an automatically following floor would need a different feature.
- Nonfunctional core scaffold: 25/27 behavior failures; corrected core: 27/27. Argument scaffold: 7/36 failures; parser: 36/36. Real file-store checks: 43/43.
- Ruling: full chaperone export is retained for reference, but restore changes only the original standing matrix — prevents unrelated room data from being reverted — cost if wrong: restoring wider concurrent room changes requires SteamVR room setup instead.
- Read-only native OpenVR floor measurement worked on the running Dream Air; no commit/restore was invoked during development. Forced tracking flags remain an explicit write blocker.
- Fresh review identified independent process sequencing and automatic rollback identity races. Added capture/commit timing regressions too: six failures (44/50) before correction. Fixed named mutation locking, post-backup tracking/height checks, origin readback confirmation, and identity-gated rollback. Final native suite: 50/50 plus original pose 35/35.
- Reviewer verified corrected files: no remaining Critical, Important or actionable Minor findings. Live chaperone apply/restore remains pending; release validation report records the boundary.
- Updated package: 50/50 floor and 35/35 pose tests passed from packaged executables; 6/6 installer cases repeated in each PowerShell version. Windows PowerShell 5.1 floor launcher measured read-only and rejected invalid height before native launch. Staging source ZIP integrity, 212 file checksums, preset metadata and unchanged official binaries verified before final packaging.

## Executable launcher update — 2026-10-10

- User requested a release EXE to open it. Ruling: provide a portable native Windows Forms dashboard around the already-authorized scripts — makes all controls accessible without adding driver behavior — cost if wrong: it still needs the complete extracted package beside the EXE.
- Launcher model scaffold: 0/9 tests; implementation: 9/9. Process runner scaffold failed its output/exit/log test (9/10); runner implemented and 10/10 passed, using an actual isolated PowerShell process and literal argument round trips.
- Elevation host tested with fake installation/rollback scripts: success, failure and invalid-action cases passed. Fixed test-only Windows PowerShell 5.1 stderr/ErrorAction handling and negative-case exit-code bookkeeping; no live privileged actions invoked.
- UI rendered and inspected; hidden sandbox smoke exercised unit conversion, a read action, asynchronous results and control re-enabling. Exposed no startup install, profile or calibration effects. Launcher requests elevation only for driver install/restore, runs helpers hidden, keeps original script guards, and preserves logs.
- Core driver/floor binaries and runtime validation boundaries remain unchanged. Full public launcher source is included with the release.
- Fresh read-only reviewer found no Critical, Important or actionable Minor launcher defects. Its packaging prerequisite is satisfied by committing all launcher source/helper/test files before generating the release and checking their presence plus the commit in Source.zip/PROVENANCE.json.
- Aggregate build repeated: 35/35 pose, 50/50 floor, 10/10 launcher and 3/3 isolated elevation-host cases passed. Final compiled UI sandbox smoke passed; the live driver hash stayed unchanged.

## Installer long-path hotfix — 2026-10-10

- The user reproduced a PowerShell 5.1 backup Copy-Item failure in a nested extraction; the failing resource path was 262 characters. Read-only inspection confirmed it stopped before the installer mutation phase and the live driver still had hash 952A01EE903ACB6AF24AFD34022A3D83C52A3A11F17C10E41EE629F861F3D632.
- Two new real installer lifecycle cases initially failed (0/2) on PowerShell 5.1. Copy-only Robocopy fixed directory resources but exposed the same legacy limit in the atomic metadata temporary file; native extended-length JSON paths fixed that too. No machine registry/policy changes.
- Final 4/4 new cases passed in both PowerShell 5.1 and 7: Play/OpenPort >260-character backup resources and byte-exact restore/empty directories, plus real exclusive-file-lock copy failures retaining original drivers, settings and the previous restore pointer. Original installer suite repeated 6/6 in each version using actual packaged driver payloads in isolated marked sandboxes.
- Fresh read-only review found no Critical, Important or actionable Minor defects. Ruling: retain backup version/layout and existing manual rollback sequencing — fixes compatibility without a separate transaction redesign — cost if wrong: an inherited restore copy failure can leave a partially restored driver; the backup remains available.
- Ruling: the supported/tested hotfix scenario is the reported nested local release folder, using Windows' built-in Robocopy — arbitrary long runtime/config roots, network shares and unusual link layouts remain untested; no live install, restore or hardware tracking test was performed. Generated package provenance is checked separately before delivery.
- Aggregate release build passed: 35/35 pose, 50/50 floor, 10/10 launcher and 3/3 elevation-host cases. Driver binary content and all floor behavior remain unchanged.
