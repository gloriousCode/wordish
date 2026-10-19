#!/usr/bin/env python3
"""Capture unmodified emulator pixels and provenance for the release gallery.

Run with the Python interpreter from the installed pebble-tool environment.
Only this checkout's build/emulator state and ports 12444–12450 are used.
Emulators remain running; this helper never stops or restarts a process.
Use --plan to inspect the shot settings without starting an emulator.
"""

import argparse
import bz2
import hashlib
import json
import os
from pathlib import Path
import queue
import shutil
import socket
import subprocess
import time
import uuid
import zipfile
from datetime import datetime, timezone
from zoneinfo import ZoneInfo


ROOT = Path(__file__).resolve().parents[1]
SDK = Path.home() / "Library/Application Support/Pebble SDK/SDKs/4.33.1"
PORTS = {"aplite": 12446, "basalt": 12447, "chalk": 12448,
         "diorite": 12449, "emery": 12444, "flint": 12450, "gabbro": 12445}
BOARDS = {
    "aplite": ("pebble-bb2", "cortex-m3", "mtd"),
    "basalt": ("pebble-snowy-bb", "cortex-m4", "spi"),
    "chalk": ("pebble-s4-bb", "cortex-m4", "spi"),
    "diorite": ("pebble-silk-bb", "cortex-m4", "mtd"),
    "emery": ("pebble-emery", "cortex-m33", "mtd"),
    "flint": ("pebble-flint", "cortex-m4", "mtd"),
    "gabbro": ("pebble-gabbro", "cortex-m33", "mtd"),
}
APP_UUID = "bf4b4493-24d6-49e1-9388-4100aa55d9ba"
# PebbleOS debounced_connection_service hides disconnects for 25 seconds.
# Allow two further seconds for the regular-timer callback and app redraw.
DISCONNECT_SETTLE_SECONDS = 27
BASE = {1: 0, 2: 2, 3: 0, 4: 0xFFFFFF, 5: 0, 6: 0, 7: 2,
        8: -1, 9: -1, 10: -1, 11: -1, 12: -1, 13: -1,
        14: 0, 15: 1, 16: 3, 17: 0, 18: 0, 95: 0, 96: 0,
        98: 7, 99: 25}
SHOTS = [
    ("01-clear-words", {}, 80, True),
    ("02-colour-rows", {8: 0xFF0000, 9: 0xFFFF00, 10: 0x00AA00,
                        98: 3, 99: 15}, 80, True),
    ("03-corner-date", {4: 0, 5: 0xFFFFFF, 6: 1, 98: 11, 99: 45}, 80, True),
    ("04-exact-time", {4: 0, 5: 0xFFFFFF, 6: 1, 95: 1, 98: 10, 99: 42}, 80, True),
    ("05-status-proof", {6: 1, 17: 1, 18: 1, 96: 1, 98: 3, 99: 15}, 15, False),
    ("06-catalan-status", {2: 0, 6: 1, 17: 1, 18: 1, 98: 0, 99: 35}, 15, False),
    ("07-long-phrase", {6: 1, 98: 11, 99: 25}, 80, True),
    ("08-double-tap", {6: 1, 98: 10, 99: 42}, 80, True),
    ("09-gesture-disabled", {6: 1, 15: 0, 98: 10, 99: 42}, 80, True),
]
MODERN_GALLERY = {
    "01-clear-words": "lagoon", "02-colour-rows": "ember",
    "03-corner-date": "sand", "04-exact-time": "orchid",
    "05-status-proof": "grove",
}


def capture_shots(platform, preset_audit=False):
    """Resolve real presets and platform-specific inputs before capture or planning."""
    modern = platform in ("emery", "gabbro")
    presets = {}
    if modern or preset_audit:
        presets = json.loads(subprocess.check_output(
            ["node", str(ROOT / "scripts/screenshot_presets.js")], text=True))
    definitions = ([(f"preset-{name}", {}, 80, True) for name in presets]
                   if preset_audit else SHOTS)
    result = []
    for name, overrides, battery, connected in definitions:
        settings = dict(BASE | overrides)
        preset = None
        if preset_audit:
            preset = name.removeprefix("preset-")
        elif modern:
            preset = MODERN_GALLERY.get(name)
        if preset:
            settings.update({int(key): value for key, value in presets[preset].items()})
        if platform in ("aplite", "diorite", "flint") and name == "02-colour-rows":
            settings.update({1: 1, 3: 1, 4: 0, 5: 0xFFFFFF})
        if platform in ("aplite", "basalt", "chalk", "diorite"):
            connected = True
        result.append((name, settings, battery, connected, preset))
    return result


def repo_path(value):
    """Reject output or input paths outside the current checkout."""
    path = (ROOT / value).resolve()
    if not path.is_relative_to(ROOT):
        raise ValueError(f"Path must stay inside this checkout: {path}")
    return path


def source_fingerprint():
    """Record source hashes without copying source into a scratch directory."""
    paths = [ROOT / "appinfo.json", ROOT / "wscript"]
    paths.extend(sorted((ROOT / "src").rglob("*")))
    paths.extend(sorted((ROOT / "resources").rglob("*")))
    return {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in paths if path.is_file()}


def ensure_emulator(platform, sdk):
    """Start or reuse only this helper's recorded, dedicated emulator."""
    state_dir = repo_path(f"build/emulator/{platform}")
    record_path = state_dir / "capture-session.json"
    port = PORTS[platform]
    if record_path.exists():
        record = json.loads(record_path.read_text())
        result = subprocess.run(["ps", "-p", str(record["pid"]), "-o", "command="],
                                capture_output=True, text=True, check=False)
        if result.returncode == 0:
            command = result.stdout.strip()
            if str(state_dir / "flash.bin") not in command or f":{port}," not in command:
                raise RuntimeError("Recorded PID belongs to a different process; refusing reuse")
            return record
    # A bind check detects unrelated listeners before any emulator is started.
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", port))
    firmware = sdk / f"sdk-core/pebble/{platform}/qemu"
    qemu = sdk / "toolchain/bin/qemu-pebble"
    kernel = firmware / "qemu_micro_flash.bin"
    seed = firmware / "qemu_spi_flash.bin.bz2"
    for path in (qemu, kernel, seed):
        if not path.is_file():
            raise FileNotFoundError(path)
    state_dir.mkdir(parents=True, exist_ok=True)
    flash = state_dir / "flash.bin"
    if not flash.exists():
        with bz2.open(seed, "rb") as source, flash.open("xb") as destination:
            shutil.copyfileobj(source, destination)
    machine, cpu, flash_interface = BOARDS[platform]
    flash_options = (f"if=none,id=spi-flash,file={flash},format=raw"
                     if flash_interface == "spi" else f"if=mtd,format=raw,file={flash}")
    command = [str(qemu), "-rtc", "base=localtime", "-serial", "null",
               "-serial", f"tcp:127.0.0.1:{port},server=on,wait=off",
               "-serial", f"file:{state_dir / 'serial.log'}", "-kernel", str(kernel),
               "-machine", machine, "-cpu", cpu,
               "-drive", flash_options, "-display", "none"]
    if platform in ("emery", "flint"):
        command.extend(["-audio", "driver=none,id=audio0"])
    environment = os.environ.copy()
    environment["DYLD_LIBRARY_PATH"] = str(sdk / "toolchain/lib")
    with (state_dir / "qemu.log").open("ab") as log:
        process = subprocess.Popen(command, cwd=ROOT, env=environment,
                                   stdin=subprocess.DEVNULL, stdout=log, stderr=log,
                                   start_new_session=True)
    record = {"pid": process.pid, "platform": platform, "port": port,
              "command": command, "started_utc": datetime.now(timezone.utc).isoformat()}
    record_path.write_text(json.dumps(record, indent=2) + "\n")
    time.sleep(5)
    if process.poll() is not None:
        raise RuntimeError(f"Emulator exited; inspect {state_dir / 'qemu.log'}")
    return record


def capture(platform, pbw, output, sdk, metadata, preset_audit=False):
    """Install the checked bundle, capture raw pixels, and exercise gestures."""
    import png
    from libpebble2.communication import PebbleConnection
    from libpebble2.communication.transports.qemu import QemuTransport, MessageTargetQemu
    from libpebble2.communication.transports.qemu.protocol import QemuBattery, QemuBluetoothConnection, QemuTap
    from libpebble2.services.appmessage import AppMessageService, Int32
    from libpebble2.services.install import AppInstaller
    from libpebble2.services.screenshot import Screenshot
    from libpebble2.protocol.system import TimeMessage, SetUTC

    session = ensure_emulator(platform, sdk)
    pebble = PebbleConnection(QemuTransport("127.0.0.1", PORTS[platform]))
    pebble.connect()
    pebble.transport.send_packet(QemuBluetoothConnection(connected=True), MessageTargetQemu())
    time.sleep(0.3)
    pebble.run_async()
    if pebble.watch_platform != platform:
        raise RuntimeError("Emulator platform does not match the requested target")
    pebble.transport.send_packet(QemuBluetoothConnection(connected=True), MessageTargetQemu())
    time.sleep(0.3)
    local_time = datetime.now(ZoneInfo("Australia/Sydney"))
    pebble.send_packet(TimeMessage(message=SetUTC(unix_time=int(local_time.timestamp()),
        utc_offset=int(local_time.utcoffset().total_seconds() // 60), tz_name="Australia/Sydney")))
    AppInstaller(pebble, str(pbw)).install()
    time.sleep(2)
    service = AppMessageService(pebble)
    acknowledgements = queue.Queue()
    service.register_handler("ack", lambda transaction, app: acknowledgements.put((transaction, True)))
    service.register_handler("nack", lambda transaction, app: acknowledgements.put((transaction, False)))
    captures = []

    def save_capture(name, pixels, settings, battery, connected, note, preset=None):
        """Save original framebuffer pixels together with their exact inputs."""
        filename = output / f"{platform}_{name}.png"
        png.from_array(pixels, mode="RGB;8").save(str(filename))
        captures.append({"file": str(filename.relative_to(ROOT)),
                         "sha256": hashlib.sha256(filename.read_bytes()).hexdigest(),
                         "captured_utc": datetime.now(timezone.utc).isoformat(),
                         "width": len(pixels[0]) // 3, "height": len(pixels),
                         "settings": settings, "battery_percent": battery,
                         "bluetooth_connected": connected, "note": note, "preset": preset})
        print(f"Captured {filename}", flush=True)
        (output / f"{platform}_manifest.json").write_text(json.dumps(
            dict(metadata, emulator=session, captures=captures), indent=2) + "\n")

    for name, settings, battery, connected, preset in capture_shots(platform, preset_audit):
        legacy_transport = platform in ("aplite", "basalt", "chalk", "diorite")
        # Older firmware rejects AppMessage traffic while simulated BT is off.
        # Apply preferences while connected, then enter the requested state.
        pebble.transport.send_packet(QemuBluetoothConnection(connected=True), MessageTargetQemu())
        time.sleep(0.3)
        pebble.transport.send_packet(QemuBattery(percent=battery, charging=False), MessageTargetQemu())
        transaction = service.send_message(uuid.UUID(APP_UUID),
                                           {key: Int32(value) for key, value in settings.items()})
        received, accepted = acknowledgements.get(timeout=10)
        if received != transaction or not accepted:
            raise RuntimeError(f"Settings not acknowledged for {platform}/{name}")
        pebble.transport.send_packet(QemuBluetoothConnection(connected=connected), MessageTargetQemu())
        settle_seconds = DISCONNECT_SETTLE_SECONDS if not connected else 1.5
        time.sleep(settle_seconds)
        pixels = Screenshot(pebble).grab_image()
        save_capture(name, pixels, settings, battery, connected,
                     "Raw emulator pixels; exact/quiet states use test keys where set. "
                     f"Date follows emulator clock. Settled for {settle_seconds}s after inputs; "
                     + ("Legacy transport stays connected for screenshots; no simulated disconnection."
                        if legacy_transport else
                        "disconnect transitions wait beyond PebbleOS's 25-second debounce.")
                     + (f" Configuration preset: {preset}." if preset else ""), preset)
        if name in ("08-double-tap", "09-gesture-disabled"):
            pebble.transport.send_packet(QemuTap(axis=QemuTap.Axis.X, direction=1), MessageTargetQemu())
            time.sleep(0.2)
            pebble.transport.send_packet(QemuTap(axis=QemuTap.Axis.X, direction=1), MessageTargetQemu())
            time.sleep(0.2)
            tapped = Screenshot(pebble).grab_image()
            save_capture(name + "-triggered", tapped, settings, battery, connected,
                         "Two real QemuTap X+ events 200 ms apart; TEST_EXACT_TIME remains zero.")
            if settings[15] and tapped == pixels:
                raise RuntimeError("Double tap did not change the framebuffer")
            if not settings[15] and tapped != pixels:
                raise RuntimeError("Disabled gesture changed the framebuffer")
            time.sleep(settings[16] + 0.5)
            restored = Screenshot(pebble).grab_image()
            save_capture(name + "-restored", restored, settings, battery, connected,
                         "Captured after the configured exact-time duration; must match pre-tap pixels.")
            if restored != pixels:
                raise RuntimeError("Watchface did not return to its pre-tap framebuffer")
    # Leave the dedicated emulator running with its captured state for inspection.
    service.shutdown()


def main():
    """Validate release identity before touching any emulator state."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pbw", default="build/fuzzy-text-two.pbw")
    parser.add_argument("--output", default="screenshots/release-1.6.0")
    parser.add_argument("--sdk", type=Path, default=SDK)
    parser.add_argument("--platform", choices=["all", *PORTS], default="all")
    parser.add_argument("--expected-version", default="1.6.0")
    parser.add_argument("--plan", action="store_true")
    parser.add_argument("--preset-audit", action="store_true",
                        help="Capture every actual configuration-page preset")
    args = parser.parse_args()
    if Path.cwd().resolve() != ROOT:
        parser.error(f"Run from the current repository: {ROOT}")
    if args.plan:
        platforms = list(PORTS) if args.platform == "all" else [args.platform]
        print(json.dumps({platform: [
            {"name": name, "settings": settings, "battery": battery, "connected": connected, "preset": preset}
            for name, settings, battery, connected, preset in capture_shots(platform, args.preset_audit)]
            for platform in platforms}, indent=2))
        return
    pbw = repo_path(args.pbw)
    output = repo_path(args.output)
    with zipfile.ZipFile(pbw) as bundle:
        app = json.loads(bundle.read("appinfo.json"))
    if app.get("uuid") != APP_UUID or app.get("versionLabel") != args.expected_version:
        parser.error("Bundle UUID/version differs from the expected release; build it first")
    platforms = list(PORTS) if args.platform == "all" else [args.platform]
    if not all(platform in app.get("targetPlatforms", []) for platform in platforms):
        parser.error("Bundle is missing a requested platform")
    metadata = {"bundle": str(pbw.relative_to(ROOT)), "bundle_metadata": app,
                "bundle_sha256": hashlib.sha256(pbw.read_bytes()).hexdigest(),
                "source_sha256": source_fingerprint(), "sdk": str(args.sdk),
                "git_head": subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
                "git_status": subprocess.check_output(["git", "status", "--short"], text=True)}
    output.mkdir(parents=True, exist_ok=True)
    for platform in platforms:
        capture(platform, pbw, output, args.sdk.resolve(), metadata, args.preset_audit)


if __name__ == "__main__":
    main()
