# TreadmillLocomotionVR Architecture Proposal

> Reviewed and updated against the shipped HeadDirectedTurning / TiptoeToJumpVR
> XInput mux and against the actual RR-Community-API source in
> `.research/RR-Community-API`. Sections marked **[resolved]** were open
> questions that existing work already answers; **[correction]** marks points
> changed from the original draft.

## Goal

Build a single Skyrim VR SKSE mod that lets the player use the RealityRunner
treadmill without running the RealityRunner desktop app or any companion app
from us.

The mod should translate treadmill movement into stable Skyrim locomotion:

- moving -> hold left stick forward
- moving fast -> hold left stick forward + sprint
- stopped -> release movement
- brief missteps/dropouts -> keep movement briefly (strictly bounded, see
  Failure Behavior)

## Evidence So Far

- RealityRunner V2 exposes an official community USB API:
  <https://github.com/Reality-Runner/RR-Community-API> (MIT licensed, so we may
  port the protocol to C++ with attribution).
- The API uses the USB serial COM port, locally `COM4`, VID/PID `1717:0202`.
- The RealityRunner desktop app must be closed while another client owns COM4.
- The official Python/pyserial demo successfully read:
  - curve `SkyrimVR2`
  - profile `SkyrimVR`
  - `numMagnets=16`
  - `maxPulses=6.0`
  - `deadzone=4222`
  - `sprintThreshold=22301`
- Our Python CSV logger captured live API output.
- The API joystick stream is curve-mapped output, not raw magnet pulses, but it
  is enough for walking/sprinting intent.
- Skyrim VR/MGO appears to behave mainly as walk vs sprint, not fine analog
  speed. (See Output Semantics — Skyrim's gamepad path actually has three
  tiers, and we should expose all three as tunables.)

## Protocol Facts (from the vendored API source)

**[correction]** The original draft described "poll `SET stream true,WIRED`".
That is not how the transport works:

- Wire framing is `NNN:<payload>\n` where `NNN` is a **zero-padded 3-digit ASCII
  byte count** of the payload (max 999), followed by `:`, the payload, and a
  trailing newline.
- `SET stream true,WIRED` is a **one-shot enable**. After it succeeds the device
  **pushes** joystick frames continuously. We do not poll — the reader thread
  simply blocks on frame reads. This answers the "is polling fast enough"
  question: there is no polling.
- A streamed joystick payload is `<joystickValue>,<sprintActive>`.
- **Do every `GET` before enabling the stream.** The reference transport's
  `send()` flushes the input buffer, writes, then reads the *next* frame as the
  response. With a stream already running, that next frame is very likely a
  joystick frame, not the command reply. Command/stream interleaving is racy —
  so the startup order must be: connect → drain → `GET curve` / `GET profiles` /
  `GET bootmode` → `SET stream true,WIRED` → read-only loop.
- Serial params: 115200 8N1, no flow control. Opening the port may assert DTR
  and reset the device, so allow ~250 ms settle and drain before the first
  command (the Python `connect()` does exactly this).

### Use the device's own configuration

`GET curve` returns far more than the draft assumed. `CurveData` carries
`sprintmode`, `sprintButton`, `invertYaxis`, `deadzone`, `sprintThreshold`,
`maxPulses`, and curve points `y1..y5`.

Therefore:

- **Do not hardcode `deadzone=4222` / `sprintThreshold=22301`.** Those are this
  user's `SkyrimVR2` curve values and will differ per user and per curve. Read
  them from the device at startup and use them as defaults; let the INI
  *override* them.
- Honor `invertYaxis` when deriving forward sign.
- The stream already provides `sprintActive`, computed by the device against its
  own threshold. Feed that into the state machine as a first-class input rather
  than only re-deriving sprint from `joystickValue` — then apply our own
  hysteresis on top for stability.

## Proposed Runtime Design

```text
RealityRunner hardware
  -> USB serial COM API  (background reader thread)
  -> thread-safe snapshot {joystickValue, sprintActive, timestampMs}
  -> locomotion intent state machine   (evaluated on the game frame hook)
  -> InputMux contribution (left stick + sprint button)
  -> mux owner's single XInput hook
  -> Skyrim VR sees left stick forward and sprint
```

## Main Components

### 1. Serial API Client

Runs on a background thread. Never blocks the Skyrim main thread.

Responsibilities:

- auto-detect RealityRunner by VID/PID if possible; otherwise use INI COM port
- open serial port at `115200`, 8N1, settle + drain
- send `GET curve`, `GET profiles`, `GET bootmode` at startup — **before** the
  stream is enabled — and log them; adopt curve values as defaults
- enable streaming once via `SET stream true,WIRED`, then read frames only
- parse `NNN:<payload>\n` frames; resync by flushing on a bad header
- expose latest `joystickValue,sprintActive` **plus a receive timestamp** in a
  thread-safe snapshot
- reconnect safely if the device disappears
- fail silent/default-off if unavailable

**[resolved] Native serial implementation.** No third-party dependency is
needed. `CreateFileW(L"\\\\.\\COM4", ...)` + `GetCommState`/`SetCommState` (DCB:
115200, 8 data bits, no parity, 1 stop bit, no flow control) +
`SetCommTimeouts` with bounded read timeouts. Plain blocking reads on the
dedicated thread are sufficient — overlapped I/O is unnecessary complexity here.
Join the thread on shutdown and close the handle.

**[resolved] VID/PID auto-detect.** `SetupDiGetClassDevs` with
`GUID_DEVINTERFACE_COMPORT`, then match the device instance ID against
`VID_1717&PID_0202` and read `PortName` from the device's registry key. That
uses `SetupAPI` from the Windows SDK — no heavy dependency. A simpler fallback
is enumerating `HKLM\HARDWARE\DEVICEMAP\SERIALCOMM`. Recommendation: INI port
setting wins; auto-detect is a convenience when it is unset.

### 2. Intent State Machine

Converts noisy treadmill output into stable movement intent. Evaluated on the
game frame hook (see Component 3) using the latest snapshot.

Initial states:

- `Stopped`
- `Walking`
- `Sprinting`

Initial rules:

- start walking after movement above deadzone for a short confirmation window
- stop walking only after no movement for a hold/release delay
- start sprinting after sustained sprint indication (device `sprintActive`
  and/or value above sprint threshold)
- stop sprinting only after sustained absence of that indication
- keep walking through short zero-value dropouts, **bounded** by
  `CoastMaxSeconds` (see Failure Behavior)

The exact values should be INI-tunable, with conservative defaults seeded from
the device's own curve.

### 3. Output Layer

**[resolved] Use the shared XInput mux — it already exists and is proven.**
This was the draft's biggest open question ("how should this coexist with
HeadDirectedTurning and TiptoeToJumpVR"). It is already solved and shipped:

- Vendor `InputMux.h` / `InputMux.cpp` **byte-identically** from
  HeadDirectedTurning (namespace changed only). The shared-block ABI — struct
  layout, magic `'IMUX'`, version `1`, 8 slots, 500 ms freshness, mapping name
  `Local\SkyrimVRInputMux_v1` — must match exactly.
- Contract: `HeadDirectedTurning/.research/xinput-mux/SPEC.md`.
- Claim a new unique mod id. Existing: HDT `0x48445431` `'HDT1'`, Tiptoe
  `0x54544A31` `'TTJ1'`. Proposed for this mod: **`0x544D4C31` `'TML1'`**.
- Call `SetContribution(rx, ry, lx, ly, buttons, leftTrigger, rightTrigger)`
  with only `ly` (forward) and the sprint bit in `buttons` set; leave the rest
  zero.
- Election is automatic: whichever participating mod loads first installs the
  single XInput hook and merges everyone's contributions (axes summed, buttons
  OR'd, triggers max'd). With 8 slots, three mods is comfortable.

**Axis ownership is clean** — HDT drives right-stick X, Tiptoe drives
right-stick Y, this mod drives left stick + a button. Nothing overlaps, so the
merge is a straight sum with no contention.

**[correction] Do not gate output on "did I install the hook".** We just fixed
exactly this bug in TiptoeToJumpVR: as a non-owner contributor it installs no
hook, so a `HasStateHook()` gate silently disabled all output and the jump never
reached the game. Gate on `HasStateHook() || InputMux::IsAvailable()` instead.
If this mod loads alone it wins the election and owns the hook; if HDT or Tiptoe
is present it is a pure contributor — both paths must produce output.

**Write the contribution from the game frame hook**, not from the serial thread.
Use the `PlayerCharacter::Update` vtable hook (slot `0xAF`) that both existing
mods use, reading the thread-safe snapshot. This matters for three reasons:

1. It lets us gate on game state (menus, loading, dialogue, paused).
2. It keeps our heartbeat cadence aligned with the owner's hook.
3. If the game pauses or hangs, we stop writing, the contribution goes stale,
   and the mux drops it — movement fails safe instead of persisting.

#### Output Semantics

**[correction] "left grip" is a VR-controller binding, not an XInput concept.**
We inject through the XInput path (a synthetic gamepad), so we cannot press a VR
grip. Sprint must be whatever **XInput button** Skyrim VR maps to Sprint. The
strong candidate is `XINPUT_GAMEPAD_LEFT_THUMB` (L3 / left-stick click, Skyrim's
default gamepad sprint), carried in the mux `buttons` field — but per this
project's standing rule, **prove it with logs before relying on it**; MGO and
VRIK rebinds can change it. Note the device's curve also has its own
`sprintButton`/`sprintmode`, which describes what the *desktop app* would have
sent — useful evidence, not automatically what we should send.

**[correction] Skyrim's gamepad locomotion has three tiers, not two.** Walk vs
run is driven by *stick magnitude*; sprint is a separate button. So expose:

```ini
[Output]
WalkStickMagnitude=0.5
RunStickMagnitude=1.0
SprintUsesButton=true
```

That lets the user match whatever MGO actually does, and collapse to two tiers
by setting both magnitudes equal. As with HeadDirectedTurning's
`MinimumStickOutput`, the walking value **must clear Skyrim's controller
deadzone** or it will read as no input at all.

State to output:

- `Stopped`: left stick neutral, sprint released
- `Walking`: stable left stick forward at `WalkStickMagnitude`
- `Sprinting`: stable left stick forward at `RunStickMagnitude` + sprint held

## Failure Behavior (new section — treat as a hard requirement)

A stuck "hold forward" is the worst possible failure of this mod: the player
keeps running in-game while standing still on the treadmill. Fail-safe rules:

- **Staleness watchdog.** If no fresh serial frame has arrived within
  `StaleTimeoutMs` (suggest 300–500 ms), force `Stopped` and zero the
  contribution. Do not rely on the mux's 500 ms freshness window for this — the
  mux only drops contributions that stop being *written*, and we would still be
  writing a stale forward value with a fresh heartbeat.
- **`CoastMaxSeconds` must be strictly less than `StaleTimeoutMs`.** The
  "keep moving through brief dropouts" behavior directly conflicts with failing
  safe; bound it explicitly so the two rules cannot argue.
- **Zero on disconnect**, on read error, on reader-thread exit, and on plugin
  shutdown.
- **Zero on game state**: menus, loading screens, dialogue, paused.
- Prefer releasing movement over holding it in every ambiguous case.

## Environment Interaction To Verify

The draft assumed closing the desktop app is sufficient. Worth confirming
explicitly, because it changes the output math:

- Does the RealityRunner present **any** joystick to Windows independently of
  its desktop app (HID gamepad, vJoy, or ViGEm virtual pad)? VID/PID
  `1717:0202` looks like a plain USB CDC serial device, which suggests the
  desktop app is what synthesizes the joystick — so closing it should remove
  that input. **Verify it.** If a real or virtual pad is still present, Skyrim
  receives the choppy signal directly and our smoothed contribution is *summed*
  on top of it, which is exactly the double-input problem we are trying to
  remove.
- Related: HeadDirectedTurning's hook only fakes slot-0 presence when no real
  pad is connected. If some other pad is connected, contributions add to that
  pad's real values rather than to zero.

## Safety Defaults

- `bEnableOutput=false`
- serial reader enabled only when explicitly configured
- no mutation until logs prove correct state transitions
- never open COM port if RealityRunner desktop app is running, unless user
  explicitly opts into contention testing
- background thread only, bounded timeouts, joined on shutdown
- clear SKSE log messages; routine per-frame lines at `debug`, one-time init at
  `info` (both sibling mods now follow this)
- no hard dependency on COM4; support config and later auto-detect

## Remaining Open Questions

The mux/coexistence and serial questions are resolved above. What genuinely
remains:

- Which XInput button does this MGO/VRIK profile actually read as Sprint?
  (Determine by logging before wiring it.)
- Does Skyrim VR's walk/run magnitude split behave the same under MGO as in flat
  Skyrim, and where is its threshold? This sets `WalkStickMagnitude`.
- Does the treadmill expose a second joystick path to Windows (see Environment
  Interaction)?
- How should the state machine treat reverse/backward, given the curve's
  `reverseMode`? V1 can ignore it; worth a decision before V2.

## Recommended First Build

**[resolved] Yes — do logging-only first.** The draft's instinct is right, and
the reasoning is now sharper: the output layer is *already solved* by the mux,
so the only genuinely new and risky part is the serial reader plus the state
machine. Prove that in isolation.

```text
COM API reader -> state machine -> log intended output
```

No actual input injection yet.

Log CSV fields:

- timestamp
- raw API payload
- joystick value
- sprint active (device)
- frame age / staleness
- state
- state transition reason
- intended stick output
- intended sprint output

Then V2: flip output on behind the INI flag, wired through the mux exactly as
described in Component 3, and test in this order — treadmill mod alone, then
with HeadDirectedTurning, then all three together.
