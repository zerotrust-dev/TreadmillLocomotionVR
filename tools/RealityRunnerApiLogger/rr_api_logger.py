import argparse
import csv
import os
import time
from dataclasses import dataclass
from datetime import datetime, timezone

import serial
from serial.tools import list_ports


RR_VID = 0x1717
RR_PID = 0x0202


@dataclass
class ApiResponse:
    command: str
    payload: str


def find_rr_port() -> str:
    found = []
    for port in list_ports.comports():
        if port.vid == RR_VID and port.pid == RR_PID:
            found.append(port.device)

    if not found:
        raise RuntimeError("No Reality Runner V2 USB device found.")
    if len(found) > 1:
        raise RuntimeError(f"Multiple Reality Runner devices found: {found}")
    return found[0]


def read_exact(ser: serial.Serial, size: int, deadline: float) -> bytes:
    data = bytearray()
    while len(data) < size:
        if time.monotonic() >= deadline:
            raise TimeoutError(f"Timed out reading {size} byte(s).")
        chunk = ser.read(size - len(data))
        if chunk:
            data.extend(chunk)
    return bytes(data)


def read_frame(ser: serial.Serial, timeout: float) -> str:
    deadline = time.monotonic() + timeout
    header = read_exact(ser, 4, deadline)
    if header[3:4] != b":":
        ser.reset_input_buffer()
        raise RuntimeError(f"Invalid frame header: {header!r}")

    try:
        length = int(header[:3].decode("ascii"))
    except ValueError as exc:
        ser.reset_input_buffer()
        raise RuntimeError(f"Invalid frame length: {header!r}") from exc

    payload = read_exact(ser, length, deadline)
    newline = read_exact(ser, 1, deadline)
    if newline != b"\n":
        ser.reset_input_buffer()
        raise RuntimeError("RealityRunner frame missing newline terminator.")
    return payload.decode("utf-8", errors="replace")


def send_command(ser: serial.Serial, command: str, timeout: float) -> ApiResponse:
    ser.reset_input_buffer()
    ser.write(command.encode("utf-8"))
    return ApiResponse(command=command.strip(), payload=read_frame(ser, timeout))


def parse_joystick(payload: str) -> tuple[int, bool]:
    parts = payload.strip().split(",")
    if len(parts) < 2:
        raise RuntimeError(f"Joystick payload had fewer than two fields: {payload!r}")
    return int(parts[0]), int(parts[1]) != 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Log Reality Runner V2 USB API joystick data to CSV.")
    parser.add_argument("--port", default="", help="Serial port, e.g. COM4. Defaults to auto-detect by VID/PID.")
    parser.add_argument("--seconds", type=float, default=120.0, help="Capture duration.")
    parser.add_argument("--poll-ms", type=float, default=50.0, help="Delay between stream polls.")
    parser.add_argument("--out", default="", help="Output CSV path.")
    parser.add_argument("--baudrate", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=2.0, help="Command response timeout in seconds.")
    args = parser.parse_args()

    if args.seconds <= 0:
        raise ValueError("--seconds must be positive.")
    if args.poll_ms <= 0:
        raise ValueError("--poll-ms must be positive.")

    port = args.port or find_rr_port()
    out_path = args.out
    if not out_path:
        stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
        out_path = os.path.join("captures", f"rr-api-{port}-{stamp}.csv")
    out_path = os.path.abspath(out_path)
    os.makedirs(os.path.dirname(out_path), exist_ok=True)

    print("RealityRunner Python API logger")
    print(f"  Port:    {port}")
    print(f"  Baud:    {args.baudrate}")
    print(f"  Seconds: {args.seconds:g}")
    print(f"  Poll:    {args.poll_ms:g} ms")
    print(f"  Output:  {out_path}")
    print()

    with serial.Serial(
        port=port,
        baudrate=args.baudrate,
        timeout=0.1,
        write_timeout=1.0,
        bytesize=serial.EIGHTBITS,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
    ) as ser:
        time.sleep(0.25)
        ser.reset_input_buffer()

        curve = send_command(ser, "GET curve\n", args.timeout).payload
        profile = send_command(ser, "GET profiles\n", args.timeout).payload
        bootmode = send_command(ser, "GET bootmode\n", args.timeout).payload

        print(f"Curve:    {curve}")
        print(f"Profile:  {profile}")
        print(f"BootMode: {bootmode}")
        print()
        print("Polling joystick stream. Move the treadmill now.")

        start = time.monotonic()
        deadline = start + args.seconds
        sample_index = 0

        with open(out_path, "w", newline="", encoding="utf-8") as csv_file:
            writer = csv.writer(csv_file)
            writer.writerow([
                "sample_index",
                "time_utc",
                "elapsed_ms",
                "joystick_value",
                "sprint_active",
                "raw_payload",
            ])

            try:
                while time.monotonic() < deadline:
                    response = send_command(ser, "SET stream true,WIRED\n", args.timeout)
                    joystick_value, sprint_active = parse_joystick(response.payload)
                    sample_index += 1
                    elapsed_ms = int((time.monotonic() - start) * 1000)
                    writer.writerow([
                        sample_index,
                        datetime.now(timezone.utc).isoformat(),
                        elapsed_ms,
                        joystick_value,
                        str(sprint_active).lower(),
                        response.payload,
                    ])
                    csv_file.flush()

                    if sample_index % 20 == 0:
                        print(
                            f"#{sample_index} {elapsed_ms} ms "
                            f"joystick={joystick_value} sprint={sprint_active}")
                    time.sleep(args.poll_ms / 1000.0)
            finally:
                try:
                    send_command(ser, "SET stream false,WIRED\n", args.timeout)
                except Exception as exc:
                    print(f"Warning: failed to disable stream cleanly: {exc}")

    print()
    print(f"Done. Captured {sample_index} sample(s).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
