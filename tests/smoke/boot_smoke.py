#!/usr/bin/env python3
"""Boot smoke test for the bare-metal kernels under QEMU (headless).

Usage:
  boot_smoke.py --qemu QEMU --kernel ELF [--machine "ARGS"] [--timeout S]
                [--expect-serial TEXT] [--screen-ref PNG [--update]]
                [--artifacts DIR]

Boots ELF with `-display none`, serial output captured to a file and the QEMU
monitor on a Unix socket, then waits until every requested check passes:

  --expect-serial TEXT  the serial output contains TEXT
  --screen-ref PNG      a monitor `screendump` equals the reference image
                        (with --update: the screen is saved as the new
                        reference once it is non-blank and has not changed
                        for STABLE_POLLS polls)

Fails if a check does not pass within --timeout seconds, if the kernel
reports a CPU exception on serial ("*** exception"), or if QEMU exits on its
own. The kernels run with -no-reboot, so a triple fault on x86_64, e.g. from
a stack overflow (the boot stack has no guard page), shows up as QEMU
exiting. On failure the serial log and the last screen (PNG) are written to
--artifacts.
"""
import argparse
import os
import re
import shlex
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import time
import zlib

POLL_INTERVAL = 0.5
STABLE_POLLS = 4          # --update: identical, non-blank screendumps in a row
EXCEPTION_MARKER = "*** exception"


# ---------------------------------------------------------------------------
# Images: (width, height, rgb bytes)
# ---------------------------------------------------------------------------

def read_ppm(path):
    with open(path, "rb") as f:
        data = f.read()
    # Header: "P6", width, height, maxval, then exactly one whitespace byte;
    # the pixels follow and may themselves start with whitespace bytes.
    m = re.match(rb"P6\s+(\d+)\s+(\d+)\s+255\s", data)
    if not m:
        raise ValueError("%s: not an 8-bit binary PPM" % path)
    w, h = int(m.group(1)), int(m.group(2))
    pixels = data[m.end():]
    if len(pixels) != w * h * 3:
        raise ValueError("%s: truncated PPM" % path)
    return w, h, pixels


def write_png(path, image):
    w, h, pixels = image
    raw = b"".join(b"\0" + pixels[y * w * 3:(y + 1) * w * 3] for y in range(h))

    def chunk(kind, body):
        return (struct.pack(">I", len(body)) + kind + body +
                struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF))

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" +
                chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
                chunk(b"IDAT", zlib.compress(raw, 9)) +
                chunk(b"IEND", b""))


def read_png(path):
    """Decodes 8-bit RGB, non-interlaced PNGs (what write_png produces)."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("%s: not a PNG" % path)
    pos, idat, header = 8, b"", None
    while pos < len(data):
        length, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", body)
        elif kind == b"IDAT":
            idat += body
        pos += 12 + length
    w, h, depth, color, _, _, interlace = header
    if (depth, color, interlace) != (8, 2, 0):
        raise ValueError("%s: only 8-bit RGB non-interlaced PNGs are supported" % path)
    raw, stride, bpp = zlib.decompress(idat), w * 3, 3
    out, prev = bytearray(), bytearray(stride)
    for y in range(h):
        ftype = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            if ftype == 1:
                line[i] = (line[i] + a) & 0xFF
            elif ftype == 2:
                line[i] = (line[i] + b) & 0xFF
            elif ftype == 3:
                line[i] = (line[i] + (a + b) // 2) & 0xFF
            elif ftype == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pred = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                line[i] = (line[i] + pred) & 0xFF
        out += line
        prev = line
    return w, h, bytes(out)


def describe_difference(actual, expected):
    aw, ah, ap = actual
    ew, eh, ep = expected
    if (aw, ah) != (ew, eh):
        return "screen is %dx%d, reference is %dx%d" % (aw, ah, ew, eh)
    diff = [i // 3 for i in range(0, len(ap), 3) if ap[i:i + 3] != ep[i:i + 3]]
    xs = [p % aw for p in diff]
    ys = [p // aw for p in diff]
    return "%d pixels differ, within x %d..%d, y %d..%d" % (
        len(diff), min(xs), max(xs), min(ys), max(ys))


# ---------------------------------------------------------------------------
# QEMU
# ---------------------------------------------------------------------------

class QemuExited(Exception):
    pass


class Monitor:
    """The QEMU human monitor on a Unix socket."""

    def __init__(self, path, qemu, deadline):
        while True:
            if qemu.poll() is not None:
                raise QemuExited()
            try:
                self.sock = socket.socket(socket.AF_UNIX)
                self.sock.settimeout(5)
                self.sock.connect(path)
                break
            except OSError:
                self.sock.close()
                if time.monotonic() > deadline:
                    raise RuntimeError("could not connect to the QEMU monitor")
                time.sleep(0.1)
        self._read_prompt()

    def _read_prompt(self):
        buf = b""
        while not buf.endswith(b"(qemu) "):
            chunk = self.sock.recv(4096)
            if not chunk:
                raise RuntimeError("QEMU monitor closed the connection")
            buf += chunk
        return buf

    def command(self, line):
        self.sock.sendall(line.encode() + b"\n")
        return self._read_prompt()

    def close(self):
        self.sock.close()


def grab_screen(monitor, dump):
    """Screendumps to `dump`; returns the image, or None if QEMU could not
    produce one yet (it then prints an error and writes no file)."""
    try:
        os.remove(dump)
    except FileNotFoundError:
        pass
    monitor.command("screendump " + dump)
    try:
        return read_ppm(dump)
    except (OSError, ValueError):
        return None


def read_text(path):
    try:
        with open(path, "rb") as f:
            return f.read().decode("latin-1")
    except FileNotFoundError:
        return ""


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--qemu", required=True)
    ap.add_argument("--kernel", required=True)
    ap.add_argument("--machine", default="", help="extra QEMU arguments (one string)")
    ap.add_argument("--timeout", type=float, default=60)
    ap.add_argument("--expect-serial")
    ap.add_argument("--screen-ref")
    ap.add_argument("--update", action="store_true",
                    help="write the stable screen to --screen-ref instead of comparing")
    ap.add_argument("--artifacts", default=".")
    args = ap.parse_args()
    if not args.expect_serial and not args.screen_ref:
        ap.error("nothing to check: give --expect-serial and/or --screen-ref")

    name = os.path.splitext(os.path.basename(args.kernel))[0]
    reference = None
    if args.screen_ref and not args.update:
        reference = read_png(args.screen_ref)

    # A private directory for QEMU's files. The monitor is a Unix socket in
    # it, so parallel runs cannot collide on a port; socket paths are limited
    # to ~104 bytes, hence the /tmp fallback for a long $TMPDIR.
    work = tempfile.mkdtemp(prefix="boot_smoke_")
    if len(os.path.join(work, "monitor.sock")) > 100:
        os.rmdir(work)
        work = tempfile.mkdtemp(prefix="boot_smoke_", dir="/tmp")
    serial_log = os.path.join(work, "serial.txt")
    qemu_log = os.path.join(work, "qemu.txt")
    monitor_sock = os.path.join(work, "monitor.sock")
    dump = os.path.join(work, "screen.ppm")
    cmd = ([args.qemu] + shlex.split(args.machine) +
           ["-kernel", args.kernel, "-display", "none", "-no-reboot",
            "-serial", "file:" + serial_log,
            "-monitor", "unix:%s,server,nowait" % monitor_sock])
    print("$ " + " ".join(shlex.quote(c) for c in cmd), flush=True)

    # QEMU's own output goes to a file: a pipe nobody reads could fill up
    # and block QEMU.
    with open(qemu_log, "wb") as log:
        qemu = subprocess.Popen(cmd, stdout=log, stderr=subprocess.STDOUT)
    deadline = time.monotonic() + args.timeout
    monitor, screen, previous_screen, stable = None, None, None, 0
    failure = None
    try:
        monitor = Monitor(monitor_sock, qemu, deadline)
        while True:
            if qemu.poll() is not None:
                raise QemuExited()
            serial = read_text(serial_log)
            if EXCEPTION_MARKER in serial:
                failure = "the kernel reported a CPU exception"
                time.sleep(POLL_INTERVAL)       # let the register dump finish
                break

            serial_ok = not args.expect_serial or args.expect_serial in serial
            screen_ok = True
            if args.screen_ref:
                grabbed = grab_screen(monitor, dump)
                if grabbed is None:
                    screen_ok = False            # display not ready: poll again
                elif args.update:
                    screen = grabbed
                    blank = screen[2].count(0) == len(screen[2])
                    stable = stable + 1 if screen == previous_screen and not blank else 0
                    previous_screen = screen
                    screen_ok = stable >= STABLE_POLLS
                else:
                    screen = grabbed
                    screen_ok = screen == reference
            if serial_ok and screen_ok:
                break
            if time.monotonic() > deadline:
                missing = []
                if not serial_ok:
                    missing.append("serial output never contained %r" % args.expect_serial)
                if not screen_ok:
                    if screen is None:
                        missing.append("QEMU never produced a screendump")
                    elif args.update:
                        missing.append("screen never stabilised")
                    else:
                        missing.append("screen never matched the reference (%s)" %
                                       describe_difference(screen, reference))
                failure = "timed out after %gs: %s" % (args.timeout, "; ".join(missing))
                break
            time.sleep(POLL_INTERVAL)
    except QemuExited:
        failure = "QEMU exited with status %d" % qemu.returncode
        if qemu.returncode == 0:
            # -no-reboot turns a guest reset into a clean exit.
            failure += (" (the guest reset: on x86_64 usually a triple fault, "
                        "e.g. a stack overflow)")
        else:
            failure += " (QEMU error, see the QEMU output below)"
    except (RuntimeError, OSError, ValueError) as e:
        failure = str(e)
    finally:
        if monitor:
            monitor.close()
        if qemu.poll() is None:
            qemu.kill()
        qemu.wait()

    serial = read_text(serial_log)
    qemu_output = read_text(qemu_log)
    shutil.rmtree(work, ignore_errors=True)
    if failure:
        os.makedirs(args.artifacts, exist_ok=True)
        with open(os.path.join(args.artifacts, name + ".serial.txt"), "w") as f:
            f.write(serial)
        print("FAIL: " + failure)
        print("--- serial output ---\n" + serial.rstrip())
        if qemu_output.strip():
            print("--- QEMU output ---\n" + qemu_output.rstrip())
        if screen:
            png = os.path.join(args.artifacts, name + ".actual.png")
            write_png(png, screen)
            print("last screen: " + png)
        return 1

    if args.update:
        write_png(args.screen_ref, screen)
        print("updated reference " + args.screen_ref)
    print("PASS: " + name)
    return 0


if __name__ == "__main__":
    sys.exit(main())
