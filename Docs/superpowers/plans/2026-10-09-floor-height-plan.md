# Floor Height Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans inline. User AGENTS.md preauthorizes approval prompts and continuation.

**Goal:** Ship fixed headset-relative floor calibration with a 1.905 m preset, custom units, readout and restore.

**Architecture:** A tested core computes a standing-origin translation from raw HMD poses. A native OpenVR adapter and durable JSON backup store perform calibration. Package launch scripts provide the user controls.

**Tech Stack:** C++17, pinned OpenVR, pinned nlohmann/json, PowerShell 5.1/7.

**Spec:** ../specs/2026-10-09-floor-height-design.md

## Global Constraints

- Preserve tracking guard and all non-standing-origin room data.
- Never calibrate the user's live floor during development.
- Fixed floor after application; exact nominal 6 ft 3 in = 1.905 m.
- Save backup before commit; identity and intervening-origin checks on restore.

## Review Focus

- Rotated/tilted origins and translation direction.
- Invalid/forced poses and movement during capture.
- Headset/universe/origin changes during capture or before restore.
- Backup and commit failures.
- Custom units, nonfinite values and conflicting arguments.

## Task

- [x] Add native tests; run against a temporary nonfunctional core and observe failures.
- [x] Implement conversion, rigid-transform math, stable capture validation, apply and restore transaction core.
- [x] Build OpenVR adapter, write-through JSON backups, read-only readout and argument handling.
- [x] Add preset/custom/read/restore scripts and packaging; test invalid input without changing runtime state.
- [x] Run all native and installer tests, perform read-only live measurement, and document runtime limitations.
- [x] Review changes and verify packaged binaries, public source, and checksums in a fresh staging package.
- [x] Preserve the release source and package the same tested code for delivery; final artifact hashes identify it.
