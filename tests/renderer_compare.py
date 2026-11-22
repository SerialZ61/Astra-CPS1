"""Compare the optimized ARM RGB565 tile primitives with a v0.1 ELF.

Usage: python tests/renderer_compare.py v01.elf candidate.elf ffight.zip
"""

import argparse
from pathlib import Path
import random
import struct

from arm_smoke import ArmCore, BMP, PCM, STR, W, H


def put(core, name, value):
    core.mu.mem_write(core.symbols[name], struct.pack("<I", value & 0xffffffff))


def pointer(core, name):
    return struct.unpack("<I", core.mu.mem_read(core.symbols[name], 4))[0]


def prepare(elf, rom, patterns):
    core = ArmCore(elf, rom)
    assert core.call("cps1_open", STR) == 0
    core.call("_Z8CtvReadyv")
    for i, data in enumerate(patterns):
        core.mu.mem_write(pointer(core, "CpsGfx") + i * 1024, data)
    palette = pointer(core, "CpsPal")
    colours = [((i * 877 + 193) & 0xffff) for i in range(2048)]
    core.mu.mem_write(palette, struct.pack("<2048I", *colours))
    put(core, "pBurnDraw", BMP)
    put(core, "CpstRowShift", PCM + 0x1000)
    for priority in (0, 1):
        table_entry = core.symbols["CpstOneDoX"] + priority * 4
        core.symbols[f"tile_{priority}"] = struct.unpack("<I", core.mu.mem_read(table_entry, 4))[0]
    return core


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("baseline", type=Path)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("rom", type=Path)
    args = parser.parse_args()
    rng = random.Random(0xC051)
    patterns = [
        bytes(1024),
        rng.randbytes(1024),
        (b"\0" * 32 + b"\x91\x01\xb0\x06" * 8) * 16,
        bytes([0xFF] * 1024),
    ]
    old = prepare(args.baseline, args.rom, patterns)
    new = prepare(args.candidate, args.rom, patterns)
    background = rng.randbytes(W * H * 2 + 64)
    sites = [(32, 42), (1, 7), (-4, -3), (379, 220), (-35, 100), (23, -40), (384, 100)]
    count = 0
    for size, size_code in ((8, 0), (16, 8), (32, 24)):
        for rows in ((0, 1) if size == 16 else (0,)):
            for flip in range(4):
                for x, y in sites:
                    for priority, mask in ((0, 0xffff), (1, 0), (1, 0xffff), (1, 0x1279)):
                        for index in range(len(patterns)):
                            care = (x != 32 or y != 42)
                            row_shifts = [rng.randrange(-7, 8) for _ in range(size)]
                            for core in (old, new):
                                core.mu.mem_write(BMP - 32, background)
                                core.mu.mem_write(PCM + 0x1000, struct.pack(f"<{size}h", *row_shifts))
                                put(core, "nCpstType", size_code + care * 2 + rows * 4)
                                put(core, "nCpstTile", index * 1024)
                                put(core, "nCpstX", x)
                                put(core, "nCpstY", y)
                                put(core, "nCpstFlip", flip)
                                put(core, "CpstPmsk", mask)
                                put(core, "CpstPal", pointer(core, "CpsPal") + 64 * (index + size))
                            a = old.call(f"tile_{priority}")
                            b = new.call(f"tile_{priority}")
                            count += 1
                            if a != b or old.mu.mem_read(BMP - 32, len(background)) != new.mu.mem_read(BMP - 32, len(background)):
                                raise RuntimeError(f"Tile differs at size={size}, rows={rows}, flip={flip}, "
                                                   f"position=({x},{y}), priority={priority}, mask={mask:#x}, "
                                                   f"pattern={index}, blank flags={a}/{b}")
    print(f"{count} tile cases match (framebuffer, guards and blank-tile return flags).")


if __name__ == "__main__":
    main()
