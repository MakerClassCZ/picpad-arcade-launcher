"""
Post-process boot2 + launcher binary for M1 layout.

Operations:
  1. Locate jump address `0x10000100` in boot2 (256 B at start of bin).
  2. Replace with `0x100F0100` (= our launcher entry point).
  3. Recompute CRC32 (RP2040 boot2 polynomial: MPEG-2 / IEEE 802.3 not-reflected).
  4. Write CRC32 to last 4 bytes of boot2.

Boot2 source (boot2_w25q080_RP2040_bin.S) contains:
  word[56] = 0x10000100  (jump target — we patch this)
  word[63] = CRC32       (we recompute this)

Usage:
  python post_process_boot2.py <bin_file>

The bin file is modified in-place.
"""
import sys
import struct
from pathlib import Path


# RP2040 boot2 CRC32 polynomial (= MPEG-2 / IEEE 802.3 non-reflected, init 0xFFFFFFFF)
# Reference: pico-sdk src/rp2_common/boot_stage2/pad_checksum
def crc32_mpeg2(data: bytes) -> int:
    crc = 0xFFFFFFFF
    for byte in data:
        crc ^= byte << 24
        for _ in range(8):
            if crc & 0x80000000:
                crc = ((crc << 1) ^ 0x04C11DB7) & 0xFFFFFFFF
            else:
                crc = (crc << 1) & 0xFFFFFFFF
    return crc


def patch_boot2(bin_path: Path) -> None:
    data = bytearray(bin_path.read_bytes())

    if len(data) < 256:
        sys.exit(f"ERROR: bin file too small ({len(data)} B), expected ≥ 256 B")

    boot2 = data[:256]

    # Boot2 jumps to (XIP_BASE + 0x100) = 0x10000100 in default layout, where
    # the vector table immediately follows boot2 (256 B = 0x100). Our launcher
    # has its vector table at 0x100F0000 (= start of LAUNCHER memory region),
    # so we patch the jump target to point there directly.
    OLD_TARGET = 0x10000100
    NEW_TARGET = patch_boot2.target  # set by main()

    old_bytes = struct.pack("<I", OLD_TARGET)
    new_bytes = struct.pack("<I", NEW_TARGET)

    occurrences = []
    for i in range(0, 252, 4):  # word-aligned, exclude CRC32 at end
        if boot2[i:i+4] == old_bytes:
            occurrences.append(i)

    if not occurrences:
        sys.exit(f"ERROR: jump target {OLD_TARGET:#x} not found in boot2")
    if len(occurrences) > 1:
        sys.exit(f"ERROR: jump target {OLD_TARGET:#x} found at multiple offsets: {occurrences}")

    offset = occurrences[0]
    print(f"  Found jump target {OLD_TARGET:#x} at boot2 offset {offset:#x} (word {offset//4})")
    print(f"  Patching to {NEW_TARGET:#x}")
    boot2[offset:offset+4] = new_bytes

    # Recompute CRC32 over first 252 bytes; boot2 contract:
    # last 4 bytes are CRC32 of first 252 bytes
    crc = crc32_mpeg2(bytes(boot2[:252]))
    old_crc = struct.unpack("<I", boot2[252:256])[0]
    print(f"  Old CRC32: {old_crc:#010x}")
    print(f"  New CRC32: {crc:#010x}")
    boot2[252:256] = struct.pack("<I", crc)

    # Write back to bin
    data[:256] = boot2
    bin_path.write_bytes(bytes(data))
    print(f"  Wrote {bin_path}")


def main():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("bin_file", type=Path)
    parser.add_argument("--target", default="0x100F0000",
                        help="boot2 jump target (= our launcher base address; "
                             "default 0x100F0000 for 1 MB layout, "
                             "use 0x101F0000 for 2 MB layout)")
    args = parser.parse_args()

    if not args.bin_file.exists():
        sys.exit(f"ERROR: file not found: {args.bin_file}")

    target = int(args.target, 0)
    if target & 0xFFF:
        sys.exit(f"ERROR: target {target:#x} not 4 KB-aligned")

    patch_boot2.target = target
    print(f"Post-processing boot2 in {args.bin_file} (target={target:#010x}):")
    patch_boot2(args.bin_file)
    print("OK")


if __name__ == "__main__":
    main()
