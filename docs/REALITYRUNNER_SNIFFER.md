# RealityRunner Sniffer

Standalone logging tool for RealityRunner's local ZeroMQ stream.

It does not touch Skyrim, XInput, COM4, or RealityRunner settings. It only
connects to local ZMQ endpoints and writes every received multipart frame to a
CSV file as UTF-8 text when printable plus hex bytes.

## Run

Start RealityRunner first, then run:

```powershell
dotnet run --project tools/RealityRunnerSniffer/RealityRunnerSniffer.csproj -- --seconds 60 --out captures/rr-zmq-test.csv
```

Default endpoints:

- `tcp://127.0.0.1:52237`
- `tcp://127.0.0.1:52239`

To test one endpoint:

```powershell
dotnet run --project tools/RealityRunnerSniffer/RealityRunnerSniffer.csproj -- --endpoint tcp://127.0.0.1:52237 --seconds 60
```

To reduce console noise during long captures:

```powershell
dotnet run --project tools/RealityRunnerSniffer/RealityRunnerSniffer.csproj -- --seconds 300 --progress-every 500
```

## Test Pattern

For the first capture:

1. Stand still.
2. Walk slowly.
3. Walk normally.
4. Run.
5. Stop.
6. Try a few backward steps.

## Output

The CSV contains:

- message index
- UTC timestamp
- elapsed milliseconds
- multipart frame index/count
- frame byte length
- printable UTF-8 text, if any
- frame hex bytes

If this stream contains `JoystickFrame`, pulse, curve, or sensor data, this
capture should reveal the message shape before we build any parser.

## Initial Smoke Test

With RealityRunner idle:

- `tcp://127.0.0.1:52237` emitted about 60 single-frame messages per second.
- Each idle frame was 508 bytes.
- The payload was binary, not printable UTF-8 text.
- `tcp://127.0.0.1:52239` emitted no messages during a short idle capture.

Use endpoint-specific captures when we need source attribution. A single ZMQ
subscriber connected to multiple endpoints can capture all messages, but it
does not identify which endpoint produced each message.

## First Treadmill Capture

Capture:
`captures/rr-zmq-treadmill-test.csv`

Observed:

- 10,590 messages over 164.3 seconds.
- Mean frame interval: 15.52 ms.
- Normal interval range: 15-16 ms, with rare 13-18 ms intervals.
- Every message had one 508-byte frame.
- Overall byte entropy was effectively 8 bits/byte.
- Every byte offset used all 256 possible byte values.
- Consecutive frames changed almost every byte.
- No plausible low-range integer, counter, joystick, pulse, or float field was
  found by simple offset scans.

Interpretation:

The `52237` ZMQ stream is real and active, but the payload is not practical
plaintext telemetry. Treat it as encrypted, compressed, or otherwise opaque
until proven otherwise. For raw treadmill pulses, the next better target is the
COM4 serial protocol or a serial proxy between the RealityRunner hardware and
companion app.
