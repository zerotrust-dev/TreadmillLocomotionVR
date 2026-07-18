# Experiment Plan

## Goal

Measure what Reality Runner / treadmill locomotion sends to Skyrim VR through
the XInput gamepad path.

## First Test

1. Install the CI artifact.
2. Set `Enabled=true` in `SKSE/Plugins/TreadmillLocomotionVR.ini`.
3. Start Skyrim VR with the Reality Runner active.
4. Record a session with:
   - standing still
   - steady walking
   - faster walking
   - running
   - faster running
5. Collect `TreadmillLocomotionVR_Telemetry.csv` from the SKSE log folder.

## What To Record

Log the **full** `XINPUT_GAMEPAD` state each poll, not just left-stick Y:

- `wButtons` (raw bitmask; this is how we discover the run button)
- `bLeftTrigger`, `bRightTrigger`
- `sThumbLX`, `sThumbLY`, `sThumbRX`, `sThumbRY`
- `dwPacketNumber`, controller index, and whether the call returned
  `ERROR_SUCCESS` (i.e. whether a pad is present at all)

Recording only left-stick Y answers how *forward* arrives but says nothing about
how *run* arrives, which is the more important unknown.

## Success Criteria

Primary question: **how does the treadmill deliver the run signal?**

- Does a controller appear on any slot while the RealityRunner desktop app is
  running? If `XInputGetState` returns `ERROR_SUCCESS` with live values, the app
  emulates a **gamepad** and run is an XInput button we can replicate.
- If no pad is present but run still works in game, the app is emulating a
  **VR controller** (or synthesizing keyboard/mouse) and the run path is
  *not* XInput, which changes our output design for running.
- Which `wButtons` bit toggles when crossing the run threshold on the
  treadmill? That bit is the button we must send.

Secondary questions about the forward axis:

- steady or noisy during constant walking
- whether it pulses on/off during a deliberately **very slow** walk (this is the
  test that decides the duty-cycle vs magnitude-oscillation hypotheses in
  ARCHITECTURE_PROPOSAL.md)
- monotonic as treadmill speed increases
- flickering around the run threshold

## Faster Pre-Checks (do these first, no build required)

1. Open the RealityRunner desktop app and read its **Sprint Button** setting.
   Gamepad-style names (A/B/LB/L3) imply a virtual gamepad; VR-style names
   (grip/trigger) imply a virtual VR controller.
2. Run `joy.cpl` with the app running. If a virtual pad is listed, open
   Properties and watch which button lights up when crossing the run
   threshold. This answers both the device type and the exact button without
   any code.
3. Run the API demo and record the `Sprint Mode` and `Sprint Button` values from
   `GET curve`.

## Non-Goals For First Build

- No smoothing.
- No run correction.
- No input replacement.
- No MCM.
