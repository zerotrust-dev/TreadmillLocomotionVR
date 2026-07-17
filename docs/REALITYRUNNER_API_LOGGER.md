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

## Run

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
