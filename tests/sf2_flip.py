"""Compare both real Street Fighter II monitor orientations on a linked ARM ELF."""
import argparse
import array
import hashlib
import json
from pathlib import Path
import struct

from arm_smoke import ArmCore, BMP, H, PCM, STR, W, rgb565


def run(elf, zip_file, out):
    out.mkdir(parents=True, exist_ok=True)
    cores = [ArmCore(elf, zip_file) for _ in range(2)]
    registers = []
    for flipped, core in enumerate(cores):
        assert core.call("cps1_open", STR) == 0
        group = next(i for i in range(core.call("cps1_dip_count"))
                     if core.cstr(core.call("cps1_dip_name", i)) == "Flip")
        assert core.call("cps1_dip_set", group, flipped) == 0
        registers.append(struct.unpack("<I", core.mu.mem_read(core.symbols["CpsSaveReg"], 4))[0] + 0x22)
    checked = 0
    frames = 2100
    for frame in range(frames):
        keys = int(285 <= frame <= 289) | (2 if 700 <= frame < 705 else 0)
        if 830 <= frame < 837 or (frame > 1300 and frame % 23 < 5):
            keys |= 64
        draw = frame % 19 == 0 or frame == 1450
        for core in cores:
            assert 0 < core.call("cps1_frame", keys, BMP if draw else 0, PCM) <= 370
        if not draw: continue
        normal, flipped = [bytes(c.mu.mem_read(BMP, W * H * 2)) for c in cores]
        video_control = [struct.unpack("<H", c.mu.mem_read(r, 2))[0] for c, r in zip(cores, registers)]
        assert video_control[0] & 0x8000 == 0
        expected = array.array("H", normal)
        if video_control[1] & 0x8000:
            expected.reverse()
            checked += 1
        assert flipped == expected.tobytes(), f"monitor raster mismatch on frame {frame}"
        if frame == 1450:
            assert flipped != normal
            rgb565(normal).save(out / "SF2-normal.png")
            rgb565(flipped).save(out / "SF2-retourne.png")
    assert checked > 90
    report = dict(elf_sha256=cores[0].elf_hash, rom_sha256=hashlib.sha256(zip_file.read_bytes()).hexdigest(),
                  frames_per_orientation=frames, flipped_frames_compared_pixelwise=checked)
    (out / "sf2-flip.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"{frames} frames per orientation, {checked} rotated rasters matched exactly.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path)
    parser.add_argument("zip", type=Path)
    parser.add_argument("out", type=Path)
    args = parser.parse_args()
    run(args.elf, args.zip, args.out)
