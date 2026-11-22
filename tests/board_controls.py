"""Exercise the linked arcade DIP tables, hardware reset and six SF2 controls.

Usage: python tests/board_controls.py out/AstraCPS1.elf Roms out/v03/validation
Requires the two original local ZIPs and the dependencies in arm_smoke.py.
"""
import argparse
import array
import hashlib
import json
from pathlib import Path
import struct

from arm_smoke import ArmCore, BMP, H, PCM, STR, W, rgb565


def exercise_dips(core):
    call = core.call
    count = call("cps1_dip_count")
    groups = {}
    defaults = [call("cps1_dip_default", i) for i in range(count)]
    for i in range(count):
        label = core.cstr(call("cps1_dip_name", i))
        options = [core.cstr(call("cps1_dip_option_name", i, j))
                   for j in range(call("cps1_dip_option_count", i))]
        assert options and len(set(options)) == len(options)
        assert defaults[i] < len(options) and defaults[i] == call("cps1_dip_get", i)
        groups[label] = {"index": i, "options": options, "default": options[defaults[i]]}
        for j in range(len(options)):
            assert call("cps1_dip_set", i, j) == 0
            assert [call("cps1_dip_get", k) for k in range(count)] == defaults[:i] + [j] + defaults[i + 1:]
        assert call("cps1_dip_set", i, len(options)) != 0
        assert call("cps1_dip_set", i, defaults[i]) == 0
    before = [call("cps1_dip_get", i) for i in range(count)]
    assert call("cps1_dip_set", 0xffffffff, 0) != 0
    assert call("cps1_dip_name", count) == 0 and call("cps1_dip_get", count) == 0xffffffff
    assert [call("cps1_dip_get", i) for i in range(count)] == before
    return groups


def switches(core):
    return [core.mu.mem_read(core.symbols[s], 1)[0] for s in ("Cpi01A", "Cpi01C", "Cpi01E")]


def audit_catalog(elf, roms):
    # Boot through the regular resolver, which also chooses the right core
    # when two driver revisions share the same printed ZIP name.
    table = {}
    for name in ("1941", "knights", "3wonders", "wonder3", "punisher", "sf2m1", "wofh"):
        archive = roms / (name + ".zip")
        if not archive.is_file():
            continue
        core = ArmCore(elf, archive)
        assert core.call("cps1_open", STR) == 0, name
        count = core.call("cps1_dip_count")
        options = 0
        for i in range(count):
            assert core.cstr(core.call("cps1_dip_name", i)), (name, i)
            size = core.call("cps1_dip_option_count", i)
            assert size > 0, (name, i)
            for j in range(size):
                assert core.cstr(core.call("cps1_dip_option_name", i, j)), (name, i, j)
            options += size
        table[name] = [count, options]
        core.call("cps1_close")
    return table


def run(elf, roms, target):
    target.mkdir(exist_ok=True, parents=True)
    report = {}
    for game in ("ffight", "sf2"):
        archive = roms / f"{game}.zip"
        core = ArmCore(elf, archive)
        call = core.call
        assert call("cps1_dip_count") == 0
        assert call("cps1_open", STR) == 0
        assert core.cstr(call("cps1_game_id", call("cps1_current_game"))) == game
        groups = exercise_dips(core)
        expected = [0, 0x0b if game == "ffight" else 3, 0x60]
        assert switches(core) == expected
        def set_dip(name, value):
            group = groups[name]
            assert call("cps1_dip_set", group["index"], group["options"].index(value)) == 0
        set_dip("Free Play", "On")
        set_dip("Demo Sound", "Off")
        set_dip("Difficulty" if game == "ffight" else "Game Level", "08" if game == "ffight" else "7")
        if game == "ffight":
            set_dip("Lives", "4")
        chosen = switches(core)
        assert chosen == ([0, 0x0f, 0x46] if game == "ffight" else [0, 7, 0x44])
        call("cps1_reset")
        assert core.mu.mem_read(core.symbols["CpsReset"], 1)[0] == 1
        call("cps1_frame", 0, 0, PCM)
        assert switches(core) == chosen and core.mu.mem_read(core.symbols["CpsReset"], 1)[0] == 0
        report[game] = {"zip_sha256": hashlib.sha256(archive.read_bytes()).hexdigest(),
                        "groups": groups, "defaults": expected, "changed_kept_after_reset": chosen}
        if game == "sf2":
            # No credit entered. The board reads Free Play at boot.
            set_dip("Demo Sound", "On")
            pcm_digest = hashlib.sha256()
            peak = 0
            hit_ports = set()
            punch = core.symbols["CpsInp001"] + 4
            kick = core.symbols["CpsInp177"]
            for frame in range(3050):
                keys = 2 if 700 <= frame <= 2400 and frame % 67 < 4 else 0
                if 850 <= frame < 2350 and frame % 59 < 4: keys |= 64
                if 2180 <= frame < 2750: keys |= 32
                if frame >= 2220 and frame % 16 < 4:
                    which = (frame // 16) % 6
                    keys |= 1 << (6 + which)
                capture = frame in (660, 920, 1540, 1650, 1920, 2100, 2300, 2510, 2680, 2900)
                pairs = call("cps1_frame", keys, BMP if capture else 0, PCM)
                assert 0 < pairs <= 370
                raw = core.mu.mem_read(PCM, pairs * 4)
                pcm_digest.update(raw)
                peak = max(peak, max(abs(sample) for sample in array.array("h", raw)))
                if frame >= 2220:
                    buttons = bytes(core.mu.mem_read(punch, 3) + core.mu.mem_read(kick, 3))
                    expected_inputs = bytes((keys >> (6 + i)) & 1 for i in range(6))
                    assert buttons == expected_inputs
                    hit_ports.update(i+1 for i, pressed in enumerate(buttons) if pressed)
                if capture:
                    rgb565(core.mu.mem_read(BMP, W*H*2)).save(target / f"SF2-{frame}.png")
            assert sorted(hit_ports) == list(range(1, 7)), hit_ports
            assert peak > 1000, peak
            report[game].update(frames=3050, six_action_inputs=sorted(hit_ports), pcm_peak=peak,
                                pcm_sha256=pcm_digest.hexdigest(), coins_inserted=0)
        else:
            report["catalog_metadata_groups_and_options"] = audit_catalog(elf, roms)
        call("cps1_close")
        assert call("cps1_dip_count") == 0
    report["elf_sha256"] = core.elf_hash
    (target / "board-controls.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("Board controls, reset and six SF2 actions verified.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path)
    parser.add_argument("roms", type=Path)
    parser.add_argument("target", type=Path)
    args = parser.parse_args()
    run(args.elf, args.roms, args.target)
