# Component release notes

Versions are independent. Tags are `c6/vX.Y.Z`, `s3/vX.Y.Z`, and
`studio/vX.Y.Z`. These first releases establish a baseline for an evolving
prototype; source commits preserve changes between releases. A version is never
reused or overwritten after publication.

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
