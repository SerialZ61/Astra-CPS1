"""Measure ARM instruction slots at boot, title and in-game frames.

Example: python tests/renderer_bench.py AstraCPS1.elf ffight.zip out/results.json
Counts translated block bytes divided by four. CPS1 is built in ARM mode.
These counts exclude the 3DS frontend and do not estimate hardware FPS.
"""

import argparse
import json
from pathlib import Path
from unicorn import UC_HOOK_BLOCK
from arm_smoke import ArmCore, STR, BMP, PCM, fight_buttons


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path)
    parser.add_argument("rom", type=Path)
    parser.add_argument("report", type=Path)
    args = parser.parse_args()
    core = ArmCore(args.elf, args.rom)
    assert core.call("cps1_open", STR) == 0
    measured = {}
    probes = (0, 120, 390, 480, 870, 1020, 1100, 1101, 1102, 1105, 1190, 1600)
    slots = 0

    def block(_, address, byte_count, data):
        nonlocal slots
        slots += byte_count // 4

    for frame in range(max(probes) + 1):
        if frame == 1101:
            # Force the same fight frame to rebuild caches as on a cold scene.
            for symbol in ("_Z13AstraTileOpenv", "_Z14AstraLowerOpenv"):
                if symbol in core.symbols:
                    core.call(symbol)
        if frame in probes:
            slots = 0
            hook = core.mu.hook_add(UC_HOOK_BLOCK, block)
            core.mu.ctl_flush_tb()
        core.call("cps1_frame", fight_buttons(frame), BMP, PCM)
        if frame in probes:
            measured[frame] = slots
            core.mu.hook_del(hook)
            core.mu.ctl_flush_tb()
            print(f"{frame}: {slots:,}", flush=True)
    args.report.write_text(json.dumps({"elf_sha256": core.elf_hash, "slots": measured}, indent=2) + "\n")


if __name__ == "__main__":
    main()
