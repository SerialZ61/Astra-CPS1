"""Validate the linked ARM ZIP resolver using local ROMs and in-memory split fixtures.

Usage: python tests/romzip_cases.py out/AstraCPS1.elf Roms
Fixtures with synthetic contents are fed only to the resolver, never to the CPU.
"""
import argparse
import gc
import io
from pathlib import Path
import struct
from zipfile import ZipFile, ZIP_DEFLATED
import zlib

from arm_smoke import ArmCore, REGS, STACK, STR, arm


class ZipCore(ArmCore):
    def __init__(self, elf, files, name):
        self.archive_bytes = files
        super().__init__(elf, Path(name))
        self.mu.mem_write(STR, ("sdmc:/roms/" + name).encode() + b"\0")

    def interrupt(self, mu, number, unused):
        pc = mu.reg_read(arm.UC_ARM_REG_PC)
        step = 2 if mu.reg_read(arm.UC_ARM_REG_CPSR) & 0x20 else 4
        name = self.patches.get(pc - step)
        if name not in ("fopen", "fopen64", "feof"):
            return super().interrupt(mu, number, unused)
        self.counts[name] += 1
        a, b, _, _ = mu.reg_read_batch(REGS)
        if name == "feof":
            stream = self.files[a]
            result = int(stream.tell() >= stream.getbuffer().nbytes)
        else:
            path = self.cstr(a)
            assert path.startswith("sdmc:/roms/") and self.cstr(b) in ("rb", "r")
            data = self.archive_bytes.get(path[len("sdmc:/roms/"):])
            result = 0
            if data is not None:
                result = max(self.files.keys(), default=0) + 1
                self.files[result] = io.BytesIO(data)
        mu.reg_write(arm.UC_ARM_REG_R0, result)


def chip_list(core, game):
    buf = STR + 2048
    result = []
    for i in range(256):
        # Fifth ARM EABI argument is on the stack.
        core.mu.mem_write(STACK, struct.pack("<I", buf + 8))
        if core.call("cps1_game_chip", game, i, buf, buf + 4): break
        _, size, crc = struct.unpack("<III", core.mu.mem_read(buf, 12))
        if size and crc:
            result.append((size, crc))
    return result


def fixture_chip(size, crc):
    """Zero-filled synthetic chip with a chosen CRC using four check bytes."""
    prefix = bytes(size - 4)
    baseline = zlib.crc32(bytes(4))
    basis = {}
    for bit in range(32):
        mask = 1 << bit
        delta = zlib.crc32(mask.to_bytes(4, "little")) ^ baseline
        while delta.bit_length() in basis:
            x, y = basis[delta.bit_length()]
            delta ^= x
            mask ^= y
        basis[delta.bit_length()] = (delta, mask)
    delta = crc ^ zlib.crc32(bytes(4), zlib.crc32(prefix))
    patch = 0
    while delta:
        x, y = basis[delta.bit_length()]
        delta ^= x
        patch ^= y
    data = prefix + patch.to_bytes(4, "little")
    assert zlib.crc32(data) == crc
    return data


def archive(chips, source):
    dest = io.BytesIO()
    with ZipFile(dest, "w", ZIP_DEFLATED, compresslevel=1) as z:
        for index, key in enumerate(sorted(chips, reverse=True)):
            data = source[key] if key in source else fixture_chip(*key)
            z.writestr(f"chip-{index}", data)
    return dest.getvalue()


def run(elf, directory):
    real = {name: (directory / name).read_bytes() for name in ("ffight.zip", "sf2.zip")}
    scout = ZipCore(elf, real, "sf2.zip")
    ids = {scout.cstr(scout.call("cps1_game_id", i)): i
           for i in range(scout.call("cps1_game_count"))}
    source = {}
    for data in real.values():
        with ZipFile(io.BytesIO(data)) as z:
            for entry in z.infolist():
                source[(entry.file_size, entry.CRC)] = z.read(entry)
    layouts = {name: set(chip_list(scout, ids[name]))
               for name in ("sf2", "sf2ua", "sf2ce", "sf2ceea", "sf2hf", "sf2t")}
    case_count = 0

    def check(label, files, filename, expected, requested=None, error_text=None):
        nonlocal case_count
        core = ZipCore(elf, files, filename)
        error = STR + 1024
        want = 0xFFFFFFFF if requested is None else ids[requested]
        actual = core.call("romzip_prepare", STR, want, error, 384)
        message = core.cstr(error)
        if expected is None:
            assert actual == 0xFFFFFFFF and message, (label, actual, message)
            if error_text: assert error_text in message, (label, message)
        else:
            assert actual == ids[expected] and not message, (label, actual, message)
            # Exercise the actual BurnExtLoadRom callback and independently
            # validate every output, including chips read from parent files.
            for index, (size, crc) in enumerate(chip_list(core, actual)):
                dest = STR + 4096
                assert size <= 1024 * 1024
                assert not core.call("romzip_load", dest, error, index)
                assert struct.unpack("<I", core.mu.mem_read(error, 4))[0] == size
                assert zlib.crc32(core.mu.mem_read(dest, size)) == crc
        core.call("romzip_release")
        case_count += 1
        print(f"{label}: {expected or message}", flush=True)
        del core
        gc.collect()

    check("real Final Fight", real, "ffight.zip", "ffight")
    check("real Street Fighter II", real, "sf2.zip", "sf2")
    check("renamed files", {"my-game.zip": archive(layouts["sf2"], source)}, "my-game.zip", "sf2")
    different = layouts["sf2ua"] - layouts["sf2"]
    split = archive(different, source)
    check("split clone", {"sf2ua.zip": split, "sf2.zip": real["sf2.zip"]}, "sf2ua.zip", "sf2ua")
    check("renamed split clone", {"region.zip": split, "sf2.zip": real["sf2.zip"]}, "region.zip", "sf2ua")
    check("missing parent", {"sf2ua.zip": split}, "sf2ua.zip", None, "sf2ua", "sf2.zip")
    merged = {"sf2.zip": archive(layouts["sf2ua"] | layouts["sf2"], source)}
    check("merged parent default", merged, "sf2.zip", "sf2")
    check("merged clone explicit", merged, "sf2.zip", "sf2ua", "sf2ua")
    check("legacy Champion Edition filename",
          {"sf2ce.zip": archive(layouts["sf2ceea"], source)}, "sf2ce.zip", "sf2ceea")
    grandparents = {
        "sf2ce.zip": archive(layouts["sf2ce"], source),
        "sf2hf.zip": archive(layouts["sf2hf"] - layouts["sf2ce"], source),
        "sf2t.zip": archive(layouts["sf2t"] - layouts["sf2hf"] - layouts["sf2ce"], source),
    }
    check("nested Turbo clone", grandparents, "sf2t.zip", "sf2t")
    shared = sorted(layouts["sf2ua"] & layouts["sf2"])[:1]
    check("common chip alone", {"unknown.zip": archive(shared, source), "sf2.zip": real["sf2.zip"]},
          "unknown.zip", None)
    bad = bytearray(real["ffight.zip"])
    with ZipFile(io.BytesIO(bad)) as z:
        target = next(entry for entry in z.infolist() if entry.file_size >= 65536)
    name_size, extra_size = struct.unpack_from("<HH", bad, target.header_offset + 26)
    offset = target.header_offset + 30 + name_size + extra_size + target.compress_size // 2
    bad[offset] ^= 0x9A
    check("corrupt payload", {"ffight.zip": bad}, "ffight.zip", None, "ffight", "CRC")
    print(f"{case_count} ARM resolver cases passed.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path)
    parser.add_argument("rom_directory", type=Path)
    args = parser.parse_args()
    run(args.elf, args.rom_directory)
