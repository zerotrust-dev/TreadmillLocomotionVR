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

## Success Criteria

The CSV shows whether left-stick Y is:

- steady or noisy during constant walking
- monotonic as treadmill speed increases
- dropping after sprint threshold
- flickering around walk/run/sprint thresholds

## Non-Goals For First Build

- No smoothing.
- No sprint correction.
- No input replacement.
- No MCM.
