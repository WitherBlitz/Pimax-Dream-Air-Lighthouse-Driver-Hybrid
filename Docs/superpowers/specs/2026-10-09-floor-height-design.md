# Headset-relative floor height

User intent: add a 6 ft 3 in floor-height preset and custom heights to the Dream Air package. Applying it places SteamVR's standing floor the requested distance below the headset's current tracked position. It is a fixed calibration, not a floor that follows the headset during play. 75 international inches is exactly 1.905 metres; runtime matrices use floats and tracking has measurement uncertainty.

Use an OpenVR companion executable and launch scripts. Modify only the standing-to-raw origin's vertical translation along its own up axis. Preserve rotation, horizontal coordinates, controllers' relationship to the headset, seated origin, play-area size and collision geometry. Do not add headset-only offsets or modify pose-guard logic.

Require SteamVR already running, successful chaperone calibration, finite rigid transforms, connected valid Running_OK HMD poses, a stable short capture, a consistent HMD serial/universe and an unchanged standing origin through the capture. Defaults and custom scripts save a durable origin backup before committing the change. Applying does not happen while building or packaging.

Restore changes only the standing origin and refuses a different headset/universe or an origin changed since application. Each adjustment has its own backup; restore undoes the most recent tool adjustment. Keep complete calibration exports for reference, but do not import unrelated room/boundary data on restore.

Provide Floor-6ft3.cmd, Floor-Custom.cmd, Floor-Read.cmd, Floor-Restore.cmd. Custom feet/inches, centimetres and metres are supported (0–10 metres). Scripts reject enabled forced-tracking flags for write actions to avoid calibrating from masked tracking failure. No administrator rights needed. SteamVR calibration persists unless room setup, another tool or a game replaces/ignores it.

Verification: native math, argument, stable-input and transaction tests with a fake runtime/storage; read-only live measurement; installer regression suite; source/package checksum verification. Do not mutate the live user's floor to test it.
