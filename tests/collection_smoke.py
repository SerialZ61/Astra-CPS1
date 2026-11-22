"""Check the ARM release against each ZIP in a local collection and record startup evidence."""
import argparse
from concurrent.futures import ProcessPoolExecutor
import json
from pathlib import Path
from arm_smoke import ArmCore, BMP, H, PCM, STR, W


def boot(job):
    elf, archive = job
    row = {"zip": archive.name}
    try:
        core = ArmCore(elf, archive)
        hint = core.call("library_game_for_path", STR)
        row["menu_title"] = core.cstr(core.call("cps1_game_title", hint)) if hint != 0xffffffff else None
        if core.call("cps1_open", STR):
            raise ValueError(core.cstr(core.call("cps1_error")))
        game = core.call("cps1_current_game")
        row["set"] = core.cstr(core.call("cps1_game_id", game))
        row["title"] = core.cstr(core.call("cps1_game_title", game))
        row["missing_samples"] = bool(core.call("cps1_missing_samples"))
        row["vertical"] = bool(core.call("cps1_game_vertical", game))
        row["dip_groups"] = core.call("cps1_dip_count")
        colors, signal = [], False
        for frame in range(420):
            draw = frame in (15, 119, 289, 419)
            buttons = 1 if frame in (314, 315) else 2 if frame in (370, 371) else 0
            pairs = core.call("cps1_frame", buttons, BMP if draw else 0, PCM)
            if not 0 < pairs <= 370:
                raise ValueError(f"invalid audio length at frame {frame}")
            signal |= any(core.mu.mem_read(PCM, pairs*4))
            if draw:
                pixels = core.mu.mem_read(BMP, W*H*2)
                colors.append(len(set(memoryview(pixels).cast("H"))))
        if max(colors) <= 1:
            raise ValueError("no visible picture in the four sampled frames")
        core.call("cps1_close")
        row.update(frames=420, colors=colors, pcm_activity=signal)
    except Exception as exc:
        row["error"] = str(exc)
    return row


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path)
    parser.add_argument("folder", type=Path)
    parser.add_argument("report", type=Path)
    args = parser.parse_args()
    jobs = [(args.elf, p) for p in sorted(args.folder.glob("*.zip"))]
    if not jobs:
        parser.error("no ZIPs in the specified folder")
    with ProcessPoolExecutor(max_workers=5) as pool:
        rows = list(pool.map(boot, jobs))
    args.report.write_text(json.dumps(rows, ensure_ascii=False, indent=1), encoding="utf-8")
    failures = [r for r in rows if "error" in r]
    for r in failures:
        print(r["zip"], r["error"])
    print(f"{len(rows)-len(failures)}/{len(rows)} boots at 420 frames")
    raise SystemExit(bool(failures))
