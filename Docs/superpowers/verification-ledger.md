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
