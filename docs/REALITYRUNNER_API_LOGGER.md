# RealityRunner API Logger

Logging-only tool for the official Reality Runner V2 USB API.

Reference source:
<https://github.com/Reality-Runner/RR-Community-API>

The official demo says to close the RealityRunner desktop app before connecting
to the device. This logger follows the same rule by default.

## Protocol

The community API documents a serial protocol at `115200` baud. Commands are
plain UTF-8 lines:

- `GET curve`
- `GET profiles`
- `GET bootmode`
- `SET stream true,WIRED`
- `SET stream false,WIRED`

Responses are framed as:

```text
NNN:<payload>\n
```

The joystick payload is:

```text
joystickValue,sprintActive
```

## Run Python Logger

Close RealityRunner first, then run with the bundled Python:

```powershell
& "C:\Users\rclae\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe" tools\RealityRunnerApiLogger\rr_api_logger.py --seconds 120 --poll-ms 50 --out captures\rr-api-test.csv
```

The Python logger uses pyserial, matching the official community API demo. It
auto-detects the Reality Runner by USB VID/PID, so `--port COM4` is optional.

## Run PowerShell Logger

Close RealityRunner first, then run:

```powershell
powershell -ExecutionPolicy Bypass -File tools\RealityRunnerApiLogger\rr-api-logger.ps1 -PortName COM4 -Seconds 120 -PollMs 50 -Out captures\rr-api-test.csv
```

Move the treadmill during the capture. The CSV contains:

- sample index
- UTC timestamp
- elapsed milliseconds
- joystick value
- sprint active
- raw payload

## Notes

This API exposes curve/profile settings and live joystick output. It may still
be after RealityRunner device-side processing, so do not assume it is raw magnet
pulse data until tested against known wheel movement.

The logger asserts DTR and RTS by default because serial libraries differ here:
the official Python demo uses pyserial, while this PowerShell tool uses .NET
`SerialPort`. If the device opens but does not answer `GET curve`, try verifying
with the official Python demo from the community API repository.

## First Official API Test

The official Python demo successfully connected locally on 2026-07-17:

- Port: `COM4`
- Curve: `SkyrimVR2`
- Sprint threshold: `22301`
- Max pulses: `6.0`
- Profile: `SkyrimVR`
- Shaft diameter: `4.19`
- Magnets: `16`
- Metric: `true`
- Boot mode: `GATT`

Movement produced joystick values such as `-6319`, `-17488`, `-23406`, and
`-20025`; sprint became active at `-23406`. Treat sign and scaling as measured
API output, not yet as final Skyrim input semantics.
