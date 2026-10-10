# Native headset and Lighthouse alignment

## What 0.3.6 changes

Pimax's native headset tracking and SteamVR's Lighthouse controllers can use different origins, orientations and room universes. The driver matches the primary station by serial number and calculates a rigid transform from that station's complete Lighthouse pose to its Pimax FloorLevel pose.

It composes the transform into each eligible Lighthouse device's world transform. The headset pose source, controller input, pose predictors, sensor pose and model offsets are preserved. Cached station callbacks are replayed through their original host after the mapping becomes available. Identity, universe changes and disconnect/reconnect invalidate stale references.

## Separately saved station layouts

On the tested setup, aligning the primary station exposed a remaining 19–21 cm controller disagreement. SteamVR and Pimax had separately saved station geometry that differed by approximately 21 cm and 5.43 degrees at the second station. Averaging the two anchors or applying an arbitrary controller offset would not solve that geometric inconsistency.

The working repair was performed separately from driver installation:

1. Back up both runtime tracking databases and room data.
2. Compare the matching Pimax universe's saved relative station geometry against its live SDK station poses.
3. With SteamVR stopped, update only the matching Steam universe's station layout, gravity tilt and tilt method from the verified Pimax layout, preserving its universe ID and unrelated records.
4. Restart SteamVR with OpenPort active and compare both station poses and paired controller poses between runtimes.

Do not copy another person's tracking database. Station identities, geometry and room data belong to that setup. The driver and installer do not automatically perform this database repair. The owner's original and corrected databases, repair scripts and restore snapshot remain local.

After the targeted repair, both station frames agreed to better than one micrometre in the software comparison. In a 20-pair capture per controller, average SDK/OpenVR positional differences were about 2.0 mm and 0.1 mm. The owner then confirmed that the controllers matched their physical positions. These are comparisons between software reports, not independent physical metrology.

## Preserved headset motion

The driver retains the 0.3.5 rotational prediction correction. PVR angular velocity must be mapped from the original tracking frame into the FloorLevel world frame and then into the headset's body frame for SteamVR's predictor. Native linear velocity is already in FloorLevel world coordinates.

The final 20-second moving capture had 1,284 valid Pimax/OpenVR samples at 90 Hz, no missed or dropped compositor frames, and a maximum GPU frame time of approximately 2.06 ms. Corrected angular velocity and 20 ms prediction agreed with the converted Pimax frame within roughly 0.016 rad/s on average. Extended game and reconnect testing remains limited.
