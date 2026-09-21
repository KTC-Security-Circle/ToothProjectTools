#!/usr/bin/env python3
import argparse, csv, math, os, select, statistics, termios, time
import cv2
import numpy as np

def percentile(values, p):
    ordered = sorted(values)
    index = max(0, math.ceil(p * len(ordered)) - 1)
    return ordered[index]

def main():
    parser = argparse.ArgumentParser(description="Measure host Photodiode-to-Camera delay")
    parser.add_argument("--device", required=True)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--camera", type=int, default=0)
    parser.add_argument("--transitions", type=int, default=60)
    parser.add_argument("--safety-margin-ms", type=float, default=5.0)
    parser.add_argument("--output", required=True)
    parser.add_argument("--projector-x", type=int, default=0)
    parser.add_argument("--projector-y", type=int, default=0)
    parser.add_argument("--projector-width", type=int, default=1920)
    parser.add_argument("--projector-height", type=int, default=1080)
    args = parser.parse_args()
    if args.transitions < 1: raise SystemExit("transitions must be positive")
    baud = getattr(termios, f"B{args.baud}", None)
    if baud is None: raise SystemExit(f"unsupported baud: {args.baud}")
    fd = os.open(args.device, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    attrs = termios.tcgetattr(fd); attrs[0] = attrs[1] = attrs[3] = 0
    attrs[2] = termios.CLOCAL | termios.CREAD | termios.CS8; attrs[4] = attrs[5] = baud
    termios.tcsetattr(fd, termios.TCSANOW, attrs); termios.tcflush(fd, termios.TCIFLUSH)
    camera = cv2.VideoCapture(args.camera)
    if not camera.isOpened(): os.close(fd); raise SystemExit(f"camera open failed: {args.camera}")
    window = "Photodiode delay measurement"
    cv2.namedWindow(window, cv2.WINDOW_NORMAL); cv2.moveWindow(window, args.projector_x, args.projector_y)
    cv2.resizeWindow(window, args.projector_width, args.projector_height)
    rows, buffer = [], b""
    try:
        for sequence in range(1, args.transitions + 1):
            expected = 1 if sequence % 2 else 0
            image = np.full((args.projector_height, args.projector_width, 3), 255 if expected else 0, np.uint8)
            cv2.imshow(window, image); cv2.waitKey(1)
            deadline = time.monotonic() + 5.0; pd_ns = None
            while time.monotonic() < deadline and pd_ns is None:
                ready, _, _ = select.select([fd], [], [], 0.05)
                if not ready: continue
                buffer += os.read(fd, 128)
                while b"\n" in buffer:
                    raw, buffer = buffer.split(b"\n", 1); line = raw.strip()
                    if line not in (b"0", b"1"): print(f"invalid line ignored: {line!r}"); continue
                    received = int(line); received_ns = time.monotonic_ns()
                    if received == expected: pd_ns = received_ns; break
            if pd_ns is None: raise RuntimeError(f"photodiode timeout at transition {sequence}")
            camera_ns = None
            while time.monotonic() < deadline:
                ok, frame = camera.read()
                if not ok: continue
                ts = time.monotonic_ns(); mean = float(frame.mean())
                if (expected == 1 and mean >= 128.0) or (expected == 0 and mean < 128.0):
                    camera_ns = ts; break
            if camera_ns is None: raise RuntimeError(f"camera transition timeout at {sequence}")
            delay_ms = (camera_ns - pd_ns) / 1_000_000.0
            rows.append((sequence, "white" if expected else "black", pd_ns, camera_ns, delay_ms))
            print(f"[{sequence:03d}] {rows[-1][1]} delay_ms={delay_ms:.3f}")
    finally:
        camera.release(); cv2.destroyAllWindows(); os.close(fd)
    with open(args.output, "w", newline="", encoding="utf-8") as output:
        writer = csv.writer(output); writer.writerow(["sequence","state","photodiode_ns","camera_ns","delay_ms"])
        writer.writerows(rows)
    delays = [row[4] for row in rows]
    mean, median, p95, p99, maximum = statistics.mean(delays), statistics.median(delays), percentile(delays,.95), percentile(delays,.99), max(delays)
    recommended = math.ceil(p99 + args.safety_margin_ms)
    print(f"count: {len(delays)}\nmean: {mean:.3f}\nmedian: {median:.3f}\np95: {p95:.3f}\np99: {p99:.3f}\nmax: {maximum:.3f}")
    print(f"recommended_guard_ms: {recommended}\ntimestamp_source: current_frame_sample_host_timestamp\ncsv: {args.output}")

if __name__ == "__main__": main()
