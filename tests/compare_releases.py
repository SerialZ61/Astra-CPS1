"""Compare two linked ARM engines on an irregularly drawn, scripted fight.

Run once per ELF with a distinct JSON output. Pass --reference OLD.json on
the second run. The ARM block counter samples work, not Old 3DS wall time.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib

from unicorn import UC_HOOK_BLOCK
from arm_smoke import ArmCore, STR, BMP, PCM, W, H, rgb565


def player(frame):
    keys = 0
    if frame in (430, 431, 450, 451): keys |= 1
    if 550 <= frame < 555 or 730 <= frame < 735: keys |= 2
    if 635 <= frame < 639: keys |= 32
    if 646 <= frame < 652 or 830 <= frame < 836: keys |= 64
    if frame >= 940:
        # Change direction and depth, scroll, jump, strike, then use the special.
        step = frame % 420
        keys |= 32 if step < 300 else 16
        if 80 <= step < 112: keys |= 4
        if 180 <= step < 231: keys |= 8
        if frame % 19 < 5: keys |= 64
        if frame % 131 < 4: keys |= 128
        if frame % 221 < 3: keys |= 64 | 128
    return keys


def sf2_player(frame):
    keys = 2 if 700 <= frame <= 2400 and frame % 67 < 4 else 0
    if 850 <= frame < 2350 and frame % 59 < 4: keys |= 64
    if 2180 <= frame < 2750: keys |= 32
    if frame >= 2220 and frame % 16 < 4: keys |= 1 << (6 + (frame // 16) % 6)
    return keys


def cabinet_player(frame):
    keys = 1 if 580 <= frame < 910 and frame % 85 < 4 else 0
    if 660 <= frame < 1500 and frame % 92 < 4: keys |= 2
    if frame >= 1040:
        keys |= 32 if frame % 420 < 310 else 16
        if frame % 18 < 5: keys |= 64
        if frame % 67 < 6: keys |= 128
    return keys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path)
    parser.add_argument("rom", type=Path)
    parser.add_argument("out", type=Path)
    parser.add_argument("--reference", type=Path)
    parser.add_argument("--sf2", action="store_true", help="World Warrior free-play round and Z80 state")
    parser.add_argument("--alternate", action="store_true", help="Different SF2 selection, inputs and camera movement")
    parser.add_argument("--cabinet", action="store_true", help="Coin/start and two-button play for CPS1 action games")
    parser.add_argument("--guile-demo", action="store_true", help="Play the original attract sequence at the airfield")
    args = parser.parse_args()
    if args.guile_demo and (not args.sf2 or args.alternate): parser.error("--guile-demo requires --sf2 alone")
    if args.alternate and not args.sf2: parser.error("--alternate requires --sf2")
    args.out.parent.mkdir(parents=True, exist_ok=True)
    core = ArmCore(args.elf, args.rom)
    assert core.call("cps1_open", STR) == 0
    work_ram, = struct.unpack("<I", core.mu.mem_read(core.symbols["CpsRamFF"], 4))
    hashes = {name: hashlib.sha256() for name in ("pixels", "stereo", "ram", "cpu")}
    if args.sf2:
        assert core.cstr(core.call("cps1_game_id", core.call("cps1_current_game"))) == "sf2"
        group = next(i for i in range(core.call("cps1_dip_count"))
                     if core.cstr(core.call("cps1_dip_name", i)) == "Free Play")
        if not args.guile_demo:
            assert core.call("cps1_dip_set", group, 1) == 0
            core.call("cps1_reset")
        zram, = struct.unpack("<I", core.mu.mem_read(core.symbols["_ZL8PsndZRam"], 4))
        hashes.update(z80=hashlib.sha256(), sound_ram=hashlib.sha256())
    reference = json.loads(args.reference.read_text()) if args.reference else None
    samples = {}
    prefix = bytearray()
    total_frames = 5200 if args.guile_demo else 3200
    # Probe eight distinct instants, each with and without a submitted picture.
    probe = {n + k for n in (1016, 1168, 1402, 1564, 1852, 2086, 2610, 2992) for k in (0, 1)}
    if args.guile_demo:
        probe = {n + k for n in (3500, 3700, 3900, 4100, 4400, 4650, 4850, 5050) for k in (0,1)}
    instruction_slots = 0

    def count_block(uc, address, size, _):
        nonlocal instruction_slots
        instruction_slots += size // 4

    for frame in range(total_frames):
        period = (1, 2, 4, 3, 1, 6, 2)[(frame // 67) % 7]
        draw = frame % period == 0
        if frame in probe:
            draw = frame % 2 == 0
            core.mu.ctl_flush_tb()
            instruction_slots = 0
            hook = core.mu.hook_add(UC_HOOK_BLOCK, count_block)
        buttons = sf2_player(frame) if args.sf2 else player(frame)
        if args.cabinet:
            buttons = cabinet_player(frame)
        if args.guile_demo: buttons = 0
        if args.alternate:
            buttons = 2 if 1020 <= frame < 1100 and frame % 25 < 3 else 0
            if 1090 <= frame < 1370 and frame % 61 < 3: buttons |= 32
            if 1165 <= frame < 1173: buttons |= 8
            if frame >= 1380 and frame % 16 < 3: buttons |= 64
            if frame > 2030:
                buttons = (32 if frame % 398 < 238 else 16) | (4 if frame % 68 < 5 else 0)
                if frame % 13 < 5: buttons |= 1 << (6 + (frame // 13) % 6)
        pairs = core.call("cps1_frame", buttons, BMP if draw else 0, PCM)
        if frame in probe:
            core.mu.hook_del(hook)
            samples[str(frame)] = {"draw": draw, "arm_slots": instruction_slots}
        assert 0 < pairs <= 370
        regs = core.mu.mem_read(core.symbols["PicoCpu"], 0x64)
        pc = struct.unpack_from("<I", regs, 0x40)[0] - struct.unpack_from("<I", regs, 0x60)[0]
        registers = regs[:0x40] + struct.pack("<I", pc & 0xffffffff) + regs[0x44:0x4c] + regs[0x58:0x5c]
        data = core.mu.mem_read(BMP, W * H * 2) if draw else b""
        chunks = [data, core.mu.mem_read(PCM, pairs * 4), core.mu.mem_read(work_ram, 65536), registers]
        if args.sf2:
            chunks += [core.mu.mem_read(core.symbols["_ZL3Z80"], 63), core.mu.mem_read(zram, 2048)]
        for digest, chunk in zip(hashes.values(), chunks):
            digest.update(chunk)
            prefix.extend(struct.pack("<I", zlib.crc32(chunk)))
        if frame == (4100 if args.guile_demo else 2086):
            rgb565(data).save(args.out.with_suffix(".png"))
    core.call("cps1_close")
    report = {"elf": core.elf_hash, "rom": hashlib.sha256(args.rom.read_bytes()).hexdigest(),
              "frames": total_frames, "digests": {k: v.hexdigest() for k, v in hashes.items()},
              "work_samples": samples}
    args.out.write_text(json.dumps(report, indent=2), encoding="utf-8")
    args.out.with_suffix(".crc").write_bytes(prefix)
    if reference is not None:
        assert reference["rom"] == report["rom"]
        saved = args.reference.with_suffix(".crc").read_bytes()
        if saved != prefix:
            index = next(i for i in range(0, len(prefix), 4) if saved[i:i+4] != prefix[i:i+4])
            raise ValueError(f"Difference at frame {index // (4 * len(hashes))}, stream {tuple(hashes)[index // 4 % len(hashes)]}")
        assert reference["digests"] == report["digests"]
        print(f"{total_frames} frames match")
        for draw in (True, False):
            def total(r):
                return sum(v["arm_slots"] for v in r["work_samples"].values() if v["draw"] == draw)
            print(f"draw={draw}, ARM slot change {100 * (total(report)/total(reference)-1):.1f}%")


if __name__ == "__main__":
    main()
