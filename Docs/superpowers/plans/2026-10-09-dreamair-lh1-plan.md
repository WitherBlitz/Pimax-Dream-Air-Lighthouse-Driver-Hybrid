# Dream Air LH1 implementation plan

> Execution: superpowers:executing-plans, inline, followed by a fresh reviewer.

Goal: deliver a compiled, reversible experimental fork with tested pose validation and useful diagnostics.
Architecture: upstream 1.3.0 plus isolated pose filter, actual HMD registration, validated hot-reload settings, standalone counters, packaging scripts.
Tech stack: C++17/OpenVR, MSVC v145, PowerShell, existing upstream Tauri/Angular GUI binary.
Spec: ../specs/2026-10-09-dreamair-lh1-design.md

Global constraints: Windows x64; preserve upstream public features; no firmware/calibration changes; no live-session interruption; defaults are conservative, smoothing off; never claim tracking-loss recovery when data is missing.

Review focus: mutex order and shutdown; quaternion sign and reference frame changes; slow drift limitations; duplicate/stale samples; installer backup/rollback with pre-existing private helper and missing settings.

1. Pose core: create tests/pose_stability_tests.cpp and src/Tracking/PoseStability.h. Run tests against a passthrough implementation to observe failures, implement validation, discontinuity handling, reacquisition, smoothing, and bounded configuration, then run all tests. Include each review focus case that concerns the filter.
2. Integration: add config parsing/default metadata, mutex-safe manager, actual PimaxLighthouse activation/deactivation registration, callback routing before forceTracking, dedicated diagnostics. Build Release|x64 with v145 and SkipPostBuild=true. No build event may copy to live SteamVR.
3. Delivery: package source/binaries/upstream GUI and private helper with notices. Add stopped-runtime install and rollback, presets, OpenVR recorder/log analysis. Exercise scripts against sandbox state, capture current baseline read-only, and write user guide plus evidence report. Fresh reviewer checks filter and scripts; resolve important findings with regression tests. Deliver ready-to-test package with honest limits.
