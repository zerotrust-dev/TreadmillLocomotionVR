# Treadmill Locomotion VR

Experimental Skyrim VR SKSE plugin for RealityRunner treadmill locomotion.

Current test build:

- Reads the RealityRunner V2 directly through the official serial API on COM4.
- Converts treadmill movement into two Skyrim states: walk and run.
- Outputs left-stick-forward for walking.
- Outputs left-stick-forward plus the gamepad run button for running.
- Can write intent telemetry for tuning, disabled by default.
- Exposes tuning through SkyUI MCM.

RealityRunner's API still uses field names such as `sprintActive` and
`sprintThreshold`; those are treated as device/protocol names only. The mod's
player-facing terminology is walk/run.

## Build

GitHub Actions is the normal build path. Artifacts are MO2-ready packages.

## Install

Install the CI artifact as a mod in MO2. For the active test phase, the packaged
INI ships ready to test:

```ini
[General]
Enabled=true
Telemetry=false
DebugLogging=false

[RealityRunner]
DirectApiEnabled=true

[Output]
EnableOutput=true
```

Close the RealityRunner desktop app before launching Skyrim VR, because this
plugin opens COM4 directly.
