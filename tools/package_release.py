#!/usr/bin/env python3
"""Collects build outputs for one PlatformIO env into release/<env>/ and writes SHA-256 sums.

Produces:
  NOVA_RIDE_<env>.bin          app image only        (flash at 0x10000)
  NOVA_RIDE_<env>.factory.bin  bootloader+partitions+boot_app0+app (flash at 0x0)
  SHA256SUMS.txt
Fails if the app image does not fit the default 4 MB partition table (0x140000 bytes).
"""
import hashlib, os, subprocess, sys
from pathlib import Path

APP_PARTITION_SIZE = 0x140000  # default.csv app0 size on a 4 MB ESP32

def sha256(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()

def find_esptool():
    cand = Path.home() / ".platformio/packages/tool-esptoolpy/esptool.py"
    if cand.exists():
        return [sys.executable, str(cand)]
    return [sys.executable, "-m", "esptool"]

def find_boot_app0():
    root = Path.home() / ".platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
    if root.exists():
        return root
    sys.exit("boot_app0.bin not found - run `pio run` first")

def main():
    if len(sys.argv) != 2:
        sys.exit("usage: package_release.py <pio-env>")
    env = sys.argv[1]
    build = Path(".pio/build") / env
    fw, bl, pt = build / "firmware.bin", build / "bootloader.bin", build / "partitions.bin"
    for p in (fw, bl, pt):
        if not p.exists():
            sys.exit(f"missing build output: {p}")
    size = fw.stat().st_size
    print(f"app image: {size} bytes of {APP_PARTITION_SIZE} ({100.0 * size / APP_PARTITION_SIZE:.1f}%)")
    if size > APP_PARTITION_SIZE:
        sys.exit("ERROR: firmware does not fit the app partition")

    out = Path("release") / env
    out.mkdir(parents=True, exist_ok=True)
    app_out = out / f"NOVA_RIDE_{env}.bin"
    factory_out = out / f"NOVA_RIDE_{env}.factory.bin"
    app_out.write_bytes(fw.read_bytes())

    cmd = find_esptool() + [
        "--chip", "esp32", "merge_bin", "-o", str(factory_out),
        "--flash_mode", "dio", "--flash_freq", "40m", "--flash_size", "4MB",
        "0x1000", str(bl), "0x8000", str(pt), "0xe000", str(find_boot_app0()), "0x10000", str(fw),
    ]
    subprocess.check_call(cmd)

    lines = [f"{sha256(p)}  {p.name}" for p in (factory_out, app_out)]
    (out / "SHA256SUMS.txt").write_text("\n".join(lines) + "\n")
    print("\n".join(lines))

if __name__ == "__main__":
    main()
