# Component release notes

Versions are independent. Tags are `c6/vX.Y.Z`, `s3/vX.Y.Z`, and
`studio/vX.Y.Z`. These first releases establish a baseline for an evolving
prototype; source commits preserve changes between releases. A version is never
reused or overwritten after publication.

## Studio 0.1.7 — 2026-11-06 (experimental pre-release)

- Collapses calibration, channel diagnostics and sensor tools behind Advanced features in Live test.
- Removes key legends and persistent selected-key highlighting from the Live test 2D and 3D previews. Other screens retain their selection indicators.
- Build verified; interactive appearance has not yet been confirmed.

## Studio 0.1.6 — 2026-11-06 (experimental pre-release)

- Hides live press and signal history panels in BLE mode; they remain available over ESP-NOW / S3.
- Build verified; visual confirmation is pending.

## Studio 0.1.5 — 2026-11-06 (experimental pre-release)

- Disables the game profile, encoder, joystick and macro controls in BLE mode, while retaining BLE Hall / Rapid Trigger, SOCD, key assignments and lighting configuration.
- BLE sensor visualization remains out of scope. Availability guards and the app compile were checked; an interactive visual test is still needed.

## C6 0.1.6 and Studio 0.1.4 — 2026-11-06 (experimental pre-releases)

- Store BLE and ESP-NOW layer profiles and lighting separately on the C6, and separate local Studio layer drafts by connection mode. Existing shared settings are copied once on migration.
- Shared Hall calibration remains common to both modes. The user verified separate key assignments across BLE and ESP-NOW mode switches. The migration cannot recover profiles overwritten before this update.
- C6 and Studio built successfully. The C6 update preserves the NVS partition; a local backup was taken before flashing.

## Studio 0.1.3 — 2026-11-06 (experimental pre-release)

- In BLE mode, analog game presets are disabled, including a previously selected preset; the UI explains that analog gamepad and encoder actions are unavailable over BLE.
- This is a presentation and selection guard; it does not add BLE analog support.

## C6 0.1.5 and Studio 0.1.2 — 2026-11-06 (experimental pre-releases)

- Studio connects to the C6 over an encrypted BLE configuration service for live layer and lighting changes and writing a single layer of supported HID assignments.
- C6 validates and atomically stores the received layer and stores lighting profiles separately; disconnect aborts incomplete transfers.
- Three BLE slots and their occupancy are displayed; names are editable local Mac labels, not host names obtained from the paired devices.
- On the user's hardware, two lighting layers changed correctly and a saved H assignment typed on macOS after a C6 restart. A solid lighting color also persisted across a C6 restart with Studio closed.
- BLE live sensor telemetry is not implemented. S3 and C6 keep separate HID profiles; switching back to ESP-NOW can replace C6 assignments with the S3 profile. Other hosts and comprehensive reconnect reliability need testing.
- BLE uses Just Works pairing; encryption does not provide MITM protection.

## C6 0.1.4 — 2026-11-05 (experimental pre-release)

- Handles successful encrypted-link events that arrive before the connection
  callback and filters known hosts by selected BLE slot while advertising.
- Build passed; actual reconnect reliability, BLE privacy-address filtering and
  multi-host switching require hardware validation.

## C6 0.1.3 — 2026-11-05 (experimental pre-release)

- Rejects a host assigned to a different BLE slot before initiating link security,
  so a previous host is less likely to occupy an empty pairing slot.
- Build passed; the previous host's reconnection contention was observed, but
  the fix and three-host switching still require physical verification.

## C6 0.1.2 — 2026-11-05 (experimental pre-release)

- Adds a two-pulse blue column indicator when selecting a BLE device slot.
- Ignores stale disconnect and encryption events belonging to old handles.
- Build passed; connection timeout was still observed in a short capture. This
  is not a verified fix for intermittent connection or three-host switching.

## C6 0.1.1 — 2026-11-05 (experimental pre-release)

- Adds three BLE host slots: hold Layer and press the left, middle, or right
  bottom-row Hall key. Empty slots open a pairing window; existing bonds are
  retained and a single existing bond is migrated to slot 1.
- Connection diagnostics now include disconnect reasons. The device resumed an
  encrypted link after flashing in one observation; this is not a reliability
  result. Three-device switching, reconnect, simultaneous keys and release on
  disconnect have not been physically validated.
- Unit tests cover chord selection, not the Bluetooth stack or host behavior.

## C6 0.1.0 — 2026-11-05 (pre-release)

- 12-channel sensor scanning, ESP-NOW and direct BLE HID, RGB effects, power
  gating experiment and Layer + KEY 0 transport switch.
- BLE keyboard output, pairing, transition effects and disconnected indication
  were observed on the hardware. Bonding after power-cycle and full profile
  compatibility need additional hardware tests.
- Requires the actual wiring and the `sdkconfig.ble` partition layout. Moving
  between partition tables can overwrite data; back up before flashing.

## S3 0.1.0 — 2026-11-05 (pre-release)

- USB CDC and keyboard/media HID receiver, per-layer profile writes, Hall
  processing, ESP-NOW sensor reception and C6 profile snapshot transmission.
- Some key output, profile persistence, Rapid Trigger and SOCD tests passed.
  ESP-NOW broadcast frames do not authenticate the sender.

## Studio 0.1.1 — 2026-11-05 (pre-release)

- Shows disconnected status on 2D/3D live and lighting views, detects a local
  Bluetooth HID device without reading keyboard events, and explains the BLE
  versus USB connection limitations in settings.
- Native build and existing unit tests passed. Live telemetry and profile/lighting
  configuration over BLE remain unimplemented; device detection does not prove
  that the physical link or individual keys are functioning.

## Studio 0.1.0 — 2026-11-05 (pre-release)

- Native Mac interface, sensor visualization, calibration, profiles, device
  configuration and lighting views.
- The current locally built app refers to the original Mac's Python and project
  paths. A release of the source is provided; a portable app package is not yet
  verified. Not signed or notarized.

## Publishing a later version

1. Make and validate the component change. Record what was tested and what was
   not. Raise only the affected component's version in `versions.json` and add
   its release notes here; use a new version for every published change.
2. Commit and push the source, with any related documentation. The Git commit
   is the durable history for intermediate changes. Exclude local flash backups,
   device bonds, logs and build directories.
3. From the clean checkout, prepare a UTF-8 notes file and any reviewed release
   assets. Run `tools/release_component.sh c6 notes.md [assets...]` (or `s3`
   or `studio`). It refuses existing versions and publishes a pre-release.
4. Do not publish the current locally built Studio binary as portable: it embeds
   machine-specific paths. Build and verify a self-contained package first.

If tag push succeeds but release creation fails, inspect the remote tag and
create the missing release manually for that same immutable tag; do not delete
or retag it. Artifact and source history must match the tag's commit.
