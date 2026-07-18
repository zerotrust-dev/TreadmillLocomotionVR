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

## Mental Model — Read This First

If you read nothing else, read this. It is the whole design in one place, and
several of the points below reverse assumptions made earlier in this document's
history.

**1. The treadmill becomes a sensor. The mod becomes the player's thumb and
grip.** We open COM4 purely to read speed and decide one thing: are we below or
above the sprint threshold? Nothing else about the treadmill's output reaches the
game.

**2. We do NOT create a virtual gamepad.** The RealityRunner desktop app drives a
ViGEm virtual Xbox 360 pad. We do not. We use the same in-process XInput import
hook as HeadDirectedTurning and TiptoeToJumpVR, via the shared mux. No ViGEmBus,
no driver, no external process. The RR desktop app must be closed anyway, since
we take COM4 — and its virtual pad disappears with it.

**3. Exactly two moving states, both constant.** Treadmill speed only *selects*
between them; it never modulates the output.

| Treadmill | Left stick Y | LB (`0x0100`) | Skyrim result |
| --- | --- | --- | --- |
| stopped | neutral | released | standing |
| below threshold (any speed) | full forward | released | walk |
| above threshold (any speed) | full forward | **held** | sprint |

**4. We cannot press a VR grip. Do not try.** The player's manual sprint is a
left-*grip* binding on the VR controller. We inject through the XInput gamepad
path, so a VR grip is not reachable from here, and no VR-controller injection
should be attempted. We send **LB** instead. This is not a compromise: in this
MGO/VRIK profile, Skyrim's Sprint action is reachable from *two* independent
routes — the player's VR grip binding, and gamepad LB — and RealityRunner has
been using the LB route all along. The in-game result is identical; only the
plumbing differs.

**5. Skyrim cannot tell the player from the mod — gameplay-wise.** It sees
"forward input" and "sprint input" and acts on them. There is one device-level
nuance: Skyrim knows the input came from a *gamepad* rather than the VR
controllers, so on-screen prompts may show Xbox glyphs. This is **already true
today**, because the RR app's virtual pad has been feeding Skyrim gamepad input
the entire time the treadmill has been in use. We are replacing one
gamepad-shaped source with a cleaner in-process one; nothing changes in how
Skyrim perceives input.

**6. Fail safe, always.** A stuck forward signal is the dangerous failure. See
Failure Behavior — it is a hard requirement, not a nice-to-have.

Restated as behavior:

- moving -> hold left stick forward
- moving fast -> hold left stick forward + LB
- stopped -> release movement and LB
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
- The RealityRunner desktop app reaches the game through a **ViGEm virtual Xbox
  360 pad** (see "How The Desktop App Reaches The Game"), holding `LB`
  continuously while moving and varying left-stick Y with speed.
- Sprint in Skyrim requires **both** a sprint input and sufficient forward
  movement. Manual play confirms full forward with no sprint input still yields
  a walk — so magnitude alone never causes running, and a single full-forward
  value plus button control is sufficient. **There is no walk/run magnitude
  tuning surface in this design.**

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

## Empirical Findings (from `captures/rr-api-test.csv`, 1068 samples)

These numbers are the basis for the tuning defaults below. Re-derive them if the
curve or profile changes.

**Sign convention — forward is negative.** Every non-zero sample is negative
(range `-18334` … `-30495`, mean `-7970`, max `0`). XInput expects *positive*
`sThumbLY` for forward, so a sign flip is required, on top of honoring the
curve's `invertYaxis`. Getting this backwards produces a mod that walks the
player backwards.

**Stream rate ~16 Hz** (62 ms median interval, min 47 / max 63). The game
renders at 72–120 Hz, so the contribution must be *held* between samples. Writing
it from the game frame hook does this naturally.

**Dropouts are a minor problem.** Zero-run histogram: two short runs of 62 ms and
124 ms, and three long runs of 3.7 s / 10.6 s / 29.9 s. The long runs are genuine
"not walking"; sustained movement was a single unbroken 341-sample (~21 s) burst.
So `CoastMaxSeconds = 0.25` comfortably covers real dropouts.

**Cause of the in-game choppiness — two competing hypotheses, unresolved.**
During continuous movement the value swings between `-18334` and `-30495` — 56%
to 93% of full scale — 16 times a second.

- *Magnitude-oscillation hypothesis:* if Skyrim's forward speed varies with
  stick magnitude, that swing produces continuous speed variation.
- *Duty-cycle (PWM) hypothesis (user observation, and the stronger one):* the
  user reports that holding the thumbstick forward yields **one constant speed**
  regardless of deflection, and that a slow treadmill walk moved them *slower*
  than that. If speed is magnitude-insensitive, the only way the device can
  render a slower walk is by pulsing the stick on/off, which is felt directly as
  choppiness. This contradicts the magnitude hypothesis.

**This capture cannot decide between them.** All 716 zero samples are accounted
for by three long idle runs (713) plus two brief dropouts (3); the movement is a
single unbroken 341-sample burst. There is no sustained on/off pulsing anywhere
in the file, so it was recorded at a pace where no slowdown was needed. A capture
taken while deliberately walking *very slowly* would confirm or kill the PWM
hypothesis outright.

**Neither answer changes the design.** Per the core requirement above, we never
reproduce treadmill speed variation — both moving states emit the same constant
forward value. This analysis is retained to explain the *existing* behavior, not
to drive our output. Resolving it is optional curiosity, not a blocker.

**The device's sprint flag chatters and needs hysteresis.** `sprint_active` fired
on 215/1068 samples across 10 transitions. During ramp-up it produced four
spurious blips of 62–124 ms before the genuine 12.96 s sprint (which was itself
clean). Observed sprint at `-22561` against `sprintThreshold=22301` confirms the
device derives sprint from magnitude ≥ threshold.

Consequences:

- On `Toggle` sprint mode those four blips would each flip sprint on/off,
  leaving it in an unpredictable state. Our mod should implement **hold**
  semantics itself regardless of the device's configured mode.
- Requiring ~200–250 ms of continuous `sprintActive` to enter Sprinting rejects
  all four blips (each ≤124 ms) and would have produced one clean sprint from
  this capture instead of five events.

Derived defaults:

```ini
CoastMaxSeconds=0.25
StaleTimeoutMs=450
SprintEnterSeconds=0.22
SprintExitSeconds=0.35
```

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

**[resolved] Sprint goes through a synthetic button, and this is confirmed
working.** Two facts settle it:

1. Per the vendor's sensor-calibration tutorial, RealityRunner sprint is
   **automatic and speed-threshold driven** — the device reaches the threshold
   and the desktop app presses a configured Sprint Button. Sprint Mode options
   are `None` ("does nothing"), `Hold` ("press and hold the Sprint Button"), and
   `Toggle` ("press the SprintButton once whenever the Sprint threshold is
   reached").
2. User-confirmed behavior: on the treadmill with the RR desktop app running,
   **sprinting happens automatically with no grip squeeze**. That proves the
   app's Sprint Button already reaches Skyrim's Sprint action in this MGO/VRIK
   profile without any VR-controller input.

So sprint is reproducible by pressing a button in the mux `buttons` field. The
user's separate "left grip while pushing the thumbstick" is a parallel VRIK/MGO
binding for manual play; it is unaffected by this mod and keeps working.

**This makes sprint parity, not polish.** We take over COM4, so the desktop app
must be closed and its automatic sprint disappears with it. If we emit only the
forward stick, the user *loses* automatic sprinting versus their current setup.

**[resolved] The button is `LB` / `XINPUT_GAMEPAD_LEFT_SHOULDER` (`0x0100`)**,
observed in `joy.cpl` on the app's virtual pad and proven to drive sprint in this
MGO/VRIK profile — it is what the working setup uses today. See "The sprint
button is LB" below for how the app gates it and why we do not copy that gating.
(Earlier guess of `LEFT_THUMB`/L3 was wrong.)

Implement **hold** semantics ourselves — hold the button while the debounced
sprint state is active — regardless of the device's configured `sprintmode`.
Copying `Toggle` would inherit the chatter problem documented above.

### The core requirement (user-stated, and it drives everything)

> The treadmill should send exactly the signals the player would send by hand.
> Walking on the treadmill at *any* speed below a threshold = pushing the left
> thumbstick forward (Skyrim walk). Walking *above* that threshold, however far
> above, = thumbstick forward + grip (Skyrim sprint). Treadmill speed only
> selects between the two. It never modulates anything.

This is a two-state emulation of manual input, not a speed translation.

**Consequence: there is no magnitude tuning surface.** Both moving states send
the *same* stick value — full forward, exactly as a thumb would — and the only
difference between them is whether the sprint button is held.

```ini
[Output]
ForwardMagnitude=1.0
```

State to output:

| State | Left stick Y | Sprint button |
| --- | --- | --- |
| `Stopped` | neutral | released |
| `Walking` | `ForwardMagnitude` (full forward) | released |
| `Sprinting` | `ForwardMagnitude` (full forward) | **held** |

Full deflection trivially clears Skyrim's controller deadzone, so
HeadDirectedTurning's `MinimumStickOutput` concern does not apply here.

**This design is deliberately immune to the choppiness debate below.** Whether
RealityRunner was pulsing the stick or swinging its magnitude, we never pass
treadmill speed variation into the game at all — we replace it with a constant.
It also makes the "is Skyrim's speed magnitude-sensitive or binary?" question
moot: we always send the same value the player would send by hand, so whichever
is true, the result matches manual play by construction.

The walk/sprint selector is already solved by the device: `sprintActive`, derived
from the device's `sprintThreshold` (tunable on the device itself), with our
hysteresis on top.

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

## How The Desktop App Reaches The Game (answered)

**The RealityRunner desktop app creates a virtual Xbox 360 controller via
ViGEm.** Evidence from the local install and machine:

- `C:\Program Files\RealityRunner\third-party-licences\vigembus_LICENCE.txt` and
  `vigemclient_LICENCE.txt` — the app bundles and uses ViGEmBus/ViGEmClient.
- `Nefarius Virtual Gamepad Emulation Bus` is installed and healthy on this
  machine, alongside phantom `Xbox 360 Controller for Windows` device entries
  (registered, not present while the app is closed).
- User-confirmed behavior: treadmill walking moves the character and crossing
  the sprint threshold makes them run, with **no thumbstick and no grip
  pressed** — i.e. something else is pressing them.

So the existing chain is:

```text
magnets -> serial (joystickValue, sprintActive)
  -> RealityRunner.exe
  -> ViGEmBus virtual Xbox 360 pad
  -> left stick Y = curve-mapped speed; sprint button held when sprintActive
  -> Skyrim VR reads it as an ordinary gamepad
```

Consequences:

- **Sprint is an XInput button.** It is replicable by us through the mux
  `buttons` field. The VR-controller scenario that would have broken the sprint
  design is ruled out.
- **This mod replaces a driver-based virtual pad with an in-process XInput
  hook** — same channel Skyrim already reads (HeadDirectedTurning proved it in
  this Pimax/OpenComposite setup), minus the ViGEmBus dependency and the
  external app.
- **No double-input problem.** The virtual pad belongs to the *app*, not the
  device; VID/PID `1717:0202` is a plain USB CDC serial endpoint. Closing the
  app to take COM4 removes the pad with it.
- Note the app also ships `openvr_api.dll`. Most likely HMD detection or an
  in-VR overlay rather than game input, given the ViGEm evidence — the XInput
  logging experiment will confirm outright.

### The sprint button is LB — and how the app gates it

Observed directly in `joy.cpl` against the app's virtual pad: **only button 5
lights up — `LB` / `XINPUT_GAMEPAD_LEFT_SHOULDER` (`0x0100`)** — and the Y axis
climbs smoothly with treadmill speed.

The surprise is that **LB is held down the entire time the user is moving**, not
only above the sprint threshold. So the app does not use the button as the
sprint trigger; it holds the button permanently and lets the *Y magnitude* gate
whether Skyrim actually engages sprint.

That works because Skyrim's sprint requires **both** a sprint input and
sufficient forward movement. The full picture, reconciling treadmill behavior
with the user's manual play:

| Situation | Forward | Sprint button | Result |
| --- | --- | --- | --- |
| Manual thumbstick | full | released | walk |
| Manual thumbstick + grip | full | held | sprint |
| Treadmill, slow | low Y | LB held | walk (sprint blocked by weak input) |
| Treadmill, fast | high Y | LB held | sprint |

**Decisive consequence:** manual play proves that *full* forward with no sprint
input still yields a walk. Therefore magnitude alone never causes running, and
we do not need to reproduce the app's magnitude gating. We control the button
directly, so we can be explicit rather than rely on that workaround:

- `Walking` -> full forward, sprint button **released**
- `Sprinting` -> full forward, sprint button **held**

This is why the single `ForwardMagnitude` in Output Semantics is correct and why
the walk/run magnitude threshold question is genuinely moot for this design.

Related behavior worth remembering: HeadDirectedTurning only fakes slot-0
presence when no pad is connected. So *today*, with the RR app running, HDT is
injecting turn onto the RR virtual pad's real state; once this mod replaces the
app, no pad exists and the mux owner fakes one instead. Both work, but they are
different paths.

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

- ~~Which XInput button is sprint?~~ **Answered:** `LB` /
  `XINPUT_GAMEPAD_LEFT_SHOULDER` (`0x0100`), observed on the app's virtual pad.
- Is Skyrim's non-sprint gait at full forward the pace the user wants for
  treadmill walking? Manual play says yes (full stick alone = walk), but confirm
  on the first output build.
- ~~Where does Skyrim VR's walk/run magnitude threshold sit?~~ **Moot.** The
  two-state design always emits full forward, so magnitude behavior cannot
  affect us. (Optional curiosity: which choppiness hypothesis was correct — see
  Empirical Findings. Not a blocker.)
- ~~Does the treadmill expose a second joystick path to Windows?~~ **Answered:**
  the virtual pad is created by the desktop app via ViGEm, not by the device, so
  closing the app removes it. See "How The Desktop App Reaches The Game".
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

## 2026-07-18 Acquisition Correction

The first logging-only SKSE test proved that the device did not keep pushing
joystick frames after a single `SET stream true,WIRED`. The DLL read exactly one
sample and then every game-frame intent row went stale.

The implementation now follows the proven Python logger behavior instead:

```text
connect -> drain -> GET curve/profiles/bootmode -> repeat SET stream true,WIRED
```

`ApiPollMs=50` controls the repeated joystick poll interval. Do not build output
logic on the one-shot continuous-push assumption unless a future firmware/API
test proves it.

Then V2: flip output on behind the INI flag, wired through the mux exactly as
described in Component 3, and test in this order — treadmill mod alone, then
with HeadDirectedTurning, then all three together.
