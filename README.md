# Treadmill Locomotion VR

Experimental Skyrim VR SKSE plugin for studying Reality Runner / treadmill
locomotion input.

The first build is intentionally logging-only:

- Hook Skyrim VR's XInput polling path.
- Record the gamepad state Skyrim receives from the treadmill path.
- Write CSV telemetry for left-stick values, packet numbers, and derived
  walk/run/sprint classifications.
- Do not mutate input.

The goal is to prove the raw signal shape before adding smoothing, hysteresis,
or replacement movement output.

## Build

GitHub Actions is the normal build path. Artifacts are MO2-ready packages.

## Install

Install the CI artifact as a mod in MO2. The default INI ships disabled:

```ini
[General]
Enabled=false
```

Set `Enabled=true` for telemetry tests.
