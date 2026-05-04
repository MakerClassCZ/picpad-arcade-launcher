"""
Convert flash binary to UF2, skipping all-0xFF (empty) 256-byte pages.

For M1 launcher layout, the bin file spans 0x10000000..0x100F0000+launcher_size,
with a large gap (0x10000100..0x100EFFFF) filled with 0xFF. This tool produces
a UF2 that only includes pages with actual data, drastically reducing UF2 size
from ~960 KB to ~125 KB.

Usage:
  python bin_to_uf2.py <input.bin> <output.uf2> [--base 0x10000000] [--family 0xE48BFF56]

UF2 block format (512 B):
  uint32_t magic_start_0   = 0x0A324655 ("UF2\\n")
  uint32_t magic_start_1   = 0x9E5D5157
  uint32_t flags           = 0x2000 (familyID present)
  uint32_t target_addr
  uint32_t payload_size    = 256
  uint32_t block_no
  uint32_t num_blocks
  uint32_t family_id       = 0xE48BFF56 (RP2040)
  uint8_t  data[476]       (first 256 used, rest padding)
  uint32_t magic_end       = 0x0AB16F30
"""
import sys
import struct
import argparse
from pathlib import Path


UF2_MAGIC_START_0 = 0x0A324655
UF2_MAGIC_START_1 = 0x9E5D5157
UF2_MAGIC_END     = 0x0AB16F30
UF2_FLAG_FAMILY_ID = 0x00002000
RP2040_FAMILY_ID  = 0xE48BFF56

PAGE_SIZE = 256


def is_empty_page(page: bytes) -> bool:
    """Page is empty if all bytes are 0xFF (erased flash) or 0x00 (objcopy gap-pad).

    Note: an all-zero page in actual code is theoretically possible but in practice
    extremely unlikely (would require 256 contiguous zero bytes in .text/.rodata,
    which has no instruction encoding for it on ARM Thumb). For .data/.rodata,
    GCC would typically place such constants in .bss anyway, which is RAM-only
    and not in the flash bin.
    """
    return all(b == 0xFF for b in page) or all(b == 0x00 for b in page)


SECTOR_SIZE = 4096
PAGES_PER_SECTOR = SECTOR_SIZE // PAGE_SIZE  # = 16


def bin_to_uf2(bin_path: Path, uf2_path: Path, base_addr: int, family_id: int,
               skip_below: int = None) -> None:
    data = bin_path.read_bytes()

    # Pad to next 256 B boundary if needed
    if len(data) % PAGE_SIZE != 0:
        pad = PAGE_SIZE - (len(data) % PAGE_SIZE)
        data += b'\xff' * pad

    total_pages = len(data) // PAGE_SIZE
    print(f"  Input bin:    {len(data)} B ({total_pages} pages)")

    # First pass: identify non-empty pages
    skip_page_below = None
    if skip_below is not None:
        skip_page_below = (skip_below - base_addr) // PAGE_SIZE

    non_empty_set = set()
    for i in range(total_pages):
        if skip_page_below is not None and i < skip_page_below:
            continue
        page = data[i*PAGE_SIZE : (i+1)*PAGE_SIZE]
        if not is_empty_page(page):
            non_empty_set.add(i)

    print(f"  Non-empty pages: {len(non_empty_set)}")

    # RP2040-E14 mitigation: a 4 KB sector with at least one non-empty page
    # must include ALL 16 pages (= zero-fill or 0xFF-fill empty pages within
    # such sectors). Otherwise the silicon bug may corrupt the partial writes.
    # Identify sectors that contain at least one non-empty page.
    used_sectors = set(p // PAGES_PER_SECTOR for p in non_empty_set)
    print(f"  Used sectors:    {len(used_sectors)} "
          f"(each is {SECTOR_SIZE} B = {PAGES_PER_SECTOR} pages)")

    # Build full page list: every page in any used sector
    pages_to_write = sorted(
        p for p in range(total_pages)
        if (p // PAGES_PER_SECTOR) in used_sectors
    )
    non_empty_pages = []
    for i in pages_to_write:
        page = data[i*PAGE_SIZE : (i+1)*PAGE_SIZE]
        # If this page is currently 0x00-padded (objcopy gap), replace with 0xFF
        # so we write proper erased state to flash.
        if is_empty_page(page):
            page = b'\xff' * PAGE_SIZE
        non_empty_pages.append((i, page))

    num_blocks = len(non_empty_pages)
    print(f"  Total blocks:    {num_blocks} pages (sector-aligned for E14)")

    # Second pass: write UF2 blocks
    blocks_out = []
    for block_no, (page_idx, page) in enumerate(non_empty_pages):
        target_addr = base_addr + page_idx * PAGE_SIZE
        block = struct.pack(
            "<IIIIIIII",
            UF2_MAGIC_START_0,
            UF2_MAGIC_START_1,
            UF2_FLAG_FAMILY_ID,
            target_addr,
            PAGE_SIZE,
            block_no,
            num_blocks,
            family_id,
        )
        block += page
        block += b'\x00' * (476 - PAGE_SIZE)        # pad data area
        block += struct.pack("<I", UF2_MAGIC_END)
        assert len(block) == 512
        blocks_out.append(block)

    uf2_data = b''.join(blocks_out)
    uf2_path.write_bytes(uf2_data)

    print(f"  Output UF2:   {len(uf2_data)} B ({num_blocks} blocks)")
    print(f"  Saved to:     {uf2_path}")


def parse_int(s: str) -> int:
    """Parse decimal or hex (0x-prefixed) integer."""
    return int(s, 0)


def main():
    parser = argparse.ArgumentParser(description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("input",  type=Path, help="input .bin file")
    parser.add_argument("output", type=Path, help="output .uf2 file")
    parser.add_argument("--base",   type=parse_int, default=0x10000000,
                        help="base flash address (default: 0x10000000)")
    parser.add_argument("--family", type=parse_int, default=RP2040_FAMILY_ID,
                        help="UF2 family ID (default: 0xE48BFF56 / RP2040)")
    parser.add_argument("--skip-below", type=parse_int, default=None,
                        help="skip all blocks targeting addresses below this "
                             "(use 0x100F0000 for update-only UF2 that preserves "
                             "previously flashed game)")
    args = parser.parse_args()

    if not args.input.exists():
        sys.exit(f"ERROR: input file not found: {args.input}")

    print(f"bin_to_uf2: {args.input} -> {args.output}")
    print(f"  Base addr:    {args.base:#010x}")
    print(f"  Family ID:    {args.family:#010x}")
    if args.skip_below is not None:
        print(f"  Skip below:   {args.skip_below:#010x}")
    bin_to_uf2(args.input, args.output, args.base, args.family,
               args.skip_below)


if __name__ == "__main__":
    main()
