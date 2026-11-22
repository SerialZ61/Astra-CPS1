"""Run the project's linked CPS1 core on an emulated ARM1176 CPU.

Usage: python -m pip install unicorn pyelftools pillow
       python tests/arm_smoke.py <unstripped 3DS ELF> <ffight.zip> <output dir>

Only file, allocation and diagnostic calls are serviced by the host. The ZIP
decoder, 68000 interpreter, Z80, graphics renderer, mixer and driver all run
as compiled ARM instructions. This test does not measure Old 3DS frame rate.
"""

import argparse
import array
import collections
import hashlib
import io
import json
from pathlib import Path
import struct
import sys
import wave
import zlib

from elftools.elf.elffile import ELFFile
from PIL import Image
from unicorn import Uc, UcError, UC_ARCH_ARM, UC_HOOK_INTR, UC_MODE_ARM
from unicorn import arm_const as arm


W, H = 384, 224
HEAP, HEAP_SIZE = 0x08000000, 56 << 20
SCRATCH, SCRATCH_SIZE = 0x0C000000, 2 << 20
STR, BMP, PCM, DONE = SCRATCH, SCRATCH + 0x1000, SCRATCH + 0x30000, SCRATCH + 0x40000
STACK = SCRATCH + SCRATCH_SIZE - 16
REGS = [arm.UC_ARM_REG_R0, arm.UC_ARM_REG_R1, arm.UC_ARM_REG_R2, arm.UC_ARM_REG_R3]


class ArmCore:
    def __init__(self, filename, archive):
        self.mu = Uc(UC_ARCH_ARM, UC_MODE_ARM)
        self.mu.ctl_set_cpu_model(arm.UC_CPU_ARM_1176)
        self.mu.reg_write(arm.UC_ARM_REG_C1_C0_2, 0x00F00000)
        self.mu.reg_write(arm.UC_ARM_REG_FPEXC, 0x40000000)
        self.mu.mem_map(HEAP, HEAP_SIZE)
        self.mu.mem_map(SCRATCH, SCRATCH_SIZE)
        self.allocated = HEAP
        self.archive = archive
        self.files = {}
        self.symbols = {}
        self.patches = {}
        self.counts = collections.Counter()
        binary = filename.read_bytes()
        self.elf_hash = hashlib.sha256(binary).hexdigest()
        with io.BytesIO(binary) as f:
            elf = ELFFile(f)
            if elf["e_machine"] != "EM_ARM" or elf.elfclass != 32:
                raise ValueError("Expected a 32-bit ARM ELF")
            segments = [s for s in elf.iter_segments() if s["p_type"] == "PT_LOAD" and s["p_memsz"]]
            start = min(s["p_vaddr"] for s in segments) & ~4095
            end = max(s["p_vaddr"] + s["p_memsz"] for s in segments)
            self.mu.mem_map(start, ((end + 4095) & ~4095) - start)
            for segment in segments:
                if segment["p_filesz"]:
                    self.mu.mem_write(segment["p_vaddr"], segment.data())
            symtab = elf.get_section_by_name(".symtab")
            for sym in symtab.iter_symbols():
                if sym["st_value"]:
                    # The compatibility object is linked after the native engine and
                    # contains private copies of some identically named variables.
                    if sym["st_info"]["bind"] == "STB_LOCAL":
                        self.symbols.setdefault(sym.name, sym["st_value"])
                    else:
                        self.symbols[sym.name] = sym["st_value"]
        # Use breakpoints in just these OS-facing routines. No Python callback
        # occurs in the per-opcode inner loop of the emulated arcade machine.
        names = ("malloc calloc realloc free memalign aligned_alloc fopen fopen64 fclose fread "
                 "fseek ftell rewind ferror feof fgetc puts printf fflush __syscall_getreent").split()
        for name in names:
            if name not in self.symbols:
                continue
            address = self.symbols[name]
            if (address & ~1) in self.patches:
                continue
            self.patches[address & ~1] = name
            thunk = bytes.fromhex("00df7047") if address & 1 else bytes.fromhex("000000ef1eff2fe1")
            self.mu.mem_write(address & ~1, thunk)
        self.sizes = {}
        self.mu.hook_add(UC_HOOK_INTR, self.interrupt)
        self.mu.mem_write(STR, archive.name.encode() + b"\0")

    def alloc(self, size, align=16):
        addr = (self.allocated + align - 1) & -align
        self.allocated = addr + max(size, 16)
        if self.allocated > HEAP + HEAP_SIZE:
            raise MemoryError(f"ARM heap exceeded {HEAP_SIZE} bytes")
        self.sizes[addr] = size
        return addr

    def cstr(self, addr):
        data = bytearray()
        while len(data) < 8192:
            chunk = self.mu.mem_read(addr + len(data), 32)
            cut = chunk.find(b"\0")
            if cut >= 0:
                return (data + chunk[:cut]).decode("utf-8", "replace")
            data.extend(chunk)
        raise ValueError("unterminated ARM string")

    def interrupt(self, mu, number, _):
        pc = mu.reg_read(arm.UC_ARM_REG_PC)
        thumb = mu.reg_read(arm.UC_ARM_REG_CPSR) & 0x20
        name = self.patches.get(pc - (2 if thumb else 4))
        a, b, c, d = mu.reg_read_batch(REGS)
        self.counts[name] += 1
        if name == "malloc":
            ret = self.alloc(a)
        elif name in ("memalign", "aligned_alloc"):
            ret = self.alloc(b, a)
        elif name == "calloc":
            ret = self.alloc(a * b)
        elif name == "realloc":
            ret = self.alloc(b)
            size = min(self.sizes.get(a, 0), b)
            if size:
                mu.mem_write(ret, bytes(mu.mem_read(a, size)))
        elif name == "free":
            ret = 0
        elif name in ("fopen", "fopen64"):
            filename, mode = self.cstr(a), self.cstr(b)
            leaf = Path(filename).name
            if Path(leaf).suffix.lower() in (".dat", ".hi", ".nv"):
                # Test with a fresh volatile cabinet: optional score/NVRAM files are absent.
                mu.reg_write(arm.UC_ARM_REG_R0, 0)
                return
            if not leaf.lower().endswith(".zip") or mode not in ("rb", "r"):
                raise ValueError(f"Unexpected file access {filename!r} {mode!r}")
            path = self.archive.parent / leaf
            ret = max(self.files, default=0) + 1 if path.is_file() else 0
            if ret:
                self.files[ret] = io.BytesIO(path.read_bytes())
        elif name == "fread":
            data = self.files[d].read(b * c)
            if data:
                mu.mem_write(a, data)
            ret = len(data) // b if b else 0
        elif name == "fseek":
            offset = b - (1 << 32) if b & (1 << 31) else b
            self.files[a].seek(offset, c)
            ret = 0
        elif name == "ftell":
            ret = self.files[a].tell()
        elif name == "rewind":
            self.files[a].seek(0)
            ret = 0
        elif name == "fclose":
            self.files[a].close()
            ret = 0
        elif name == "fgetc":
            data = self.files[a].read(1)
            ret = data[0] if data else -1
        elif name == "feof":
            ret = int(self.files[a].tell() >= self.files[a].getbuffer().nbytes)
        elif name in ("puts", "printf", "fflush", "ferror"):
            ret = 0
        elif name == "__syscall_getreent":
            ret = self.symbols["_impure_data"]
        else:
            raise RuntimeError(f"Unhandled ARM interrupt {number} at {pc:#x}")
        mu.reg_write(arm.UC_ARM_REG_R0, ret & 0xFFFFFFFF)

    def call(self, name, *args):
        pc = self.symbols[name]
        for reg, value in zip(REGS, args):
            self.mu.reg_write(reg, value)
        self.mu.reg_write(arm.UC_ARM_REG_SP, STACK)
        self.mu.reg_write(arm.UC_ARM_REG_LR, DONE)
        try:
            # A timeout creates an extra host timer thread for every call in
            # Unicorn. Keep it on the startup path, outside the frame loop.
            self.mu.emu_start(pc, DONE, timeout=120_000_000 if name == "cps1_open" else 0)
        except UcError as exc:
            raise RuntimeError(f"{name} failed at ARM PC {self.mu.reg_read(arm.UC_ARM_REG_PC):#x}") from exc
        if self.mu.reg_read(arm.UC_ARM_REG_PC) != DONE:
            raise TimeoutError(f"{name} did not return")
        return self.mu.reg_read(arm.UC_ARM_REG_R0)


def rgb565(data):
    words = array.array("H")
    words.frombytes(data)
    if sys.byteorder != "little":
        words.byteswap()
    rgb = bytearray()
    for pixel in words:
        rgb.extend(((pixel >> 11) * 255 // 31, (pixel >> 5 & 63) * 255 // 63, (pixel & 31) * 255 // 31))
    return Image.frombytes("RGB", (W, H), bytes(rgb))


def fight_buttons(frame):
    buttons = 0
    if 285 <= frame < 289: buttons |= 1  # coin
    if 305 <= frame < 309 or 700 <= frame < 704: buttons |= 2
    if 365 <= frame < 369 or 830 <= frame < 834: buttons |= 64
    if 1010 <= frame < 1750: buttons |= 32  # walk right
    if 1010 <= frame < 1860 and frame % 20 < 4: buttons |= 64  # attack
    if 1040 <= frame < 1780 and frame % 77 < 4: buttons |= 128  # jump
    return buttons


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path)
    parser.add_argument("rom", type=Path)
    parser.add_argument("out", type=Path)
    parser.add_argument("--idle", action="store_true", help="Leave controls untouched for a reference run.")
    parser.add_argument("--compare-idle", type=Path,
                        help="Compare this playthrough with an idle run's report.json.")
    parser.add_argument("--original-cpu", action="store_true",
                        help="Disable Final Fight's idle-loop accelerator.")
    parser.add_argument("--compare-cpu", type=Path,
                        help="Compare every frame with an original CPU run's report.json.")
    parser.add_argument("--draw-period", type=int, choices=(1, 2, 3), default=1)
    parser.add_argument("--gif", type=Path, help="Save seven seconds of the first stage at 14.9 fps.")
    parser.add_argument("--frames", type=int, default=1900)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    core = ArmCore(args.elf, args.rom)
    if core.call("cps1_open", STR) != 0:
        raise ValueError(core.cstr(core.call("cps1_error")))
    if args.original_cpu:
        core.call("astraIdleEnable", 0)
    print("Opened original ZIP through the ARM engine", flush=True)
    samples, nonzero, peak = 0, 0, 0
    captures = {}
    pc_counts = collections.Counter()
    digests = {key: hashlib.sha256() for key in ("video", "pcm", "work_ram", "m68000")}
    frame_checks = bytearray()
    gif_frames = []
    ram_ptr = struct.unpack("<I", core.mu.mem_read(core.symbols["CpsRamFF"], 4))[0]
    capture_frames = (120, 240, 284, 300, 340, 390, 480, 600, 720, 870, 1020, 1190, 1399, 1600)
    with wave.open(str(args.out / "sound.wav"), "wb") as wav:
        wav.setparams((2, 2, 22050, 0, "NONE", ""))
        for frame in range(args.frames):
            buttons = 0 if args.idle else fight_buttons(frame)
            capture = frame in capture_frames or frame == args.frames - 1
            clip_frame = args.gif and 1180 <= frame < 1600 and frame % 4 == 0
            draw = frame % args.draw_period == 0 or capture or clip_frame
            count = core.call("cps1_frame", buttons, BMP if draw else 0, PCM)
            regs = core.mu.mem_read(core.symbols["PicoCpu"], 0x64)
            pc = struct.unpack_from("<I", regs, 0x40)[0] - struct.unpack_from("<I", regs, 0x60)[0]
            if frame >= 600 and "PicoCpu" in core.symbols:
                # Record the 68000 PC after VBlank work. This is a once-per-frame
                # sample, not a cycle-weighted profile or evidence of a safe idle loop.
                pc_counts[f"{pc & 0xFFFFFF:06x}"] += 1
            if not 0 < count <= 370:
                raise ValueError(f"Invalid audio length in frame {frame}: {count}")
            pcm = bytes(core.mu.mem_read(PCM, count * 4))
            data = core.mu.mem_read(BMP, W * H * 2) if draw else b""
            machine = regs[:0x40] + struct.pack("<I", pc & 0xFFFFFFFF) + regs[0x44:0x4C] + regs[0x58:0x5C]
            parts = (data, pcm, core.mu.mem_read(ram_ptr, 65536), machine)
            for key, part in zip(digests, parts):
                digests[key].update(part)
                frame_checks.extend(struct.pack("<I", zlib.crc32(part)))
            wav.writeframesraw(pcm)
            values = array.array("h", pcm)
            frame_peak = max(abs(v) for v in values)
            nonzero += int(frame_peak > 0)
            peak = max(peak, frame_peak)
            samples += count
            if capture:
                rgb565(data).save(args.out / f"frame-{frame:04}.png")
                captures[frame] = hashlib.sha256(data).hexdigest()
                print(f"Frame {frame}   PCM peak {frame_peak}", flush=True)
            if clip_frame:
                gif_frames.append(rgb565(data).quantize(colors=256, method=Image.Quantize.MEDIANCUT))
    if args.gif:
        if len(gif_frames) != 105:
            raise ValueError("The GIF capture needs at least 1600 simulation frames")
        args.gif.parent.mkdir(parents=True, exist_ok=True)
        # GIF times are integral 10 ms. Alternate durations to preserve pacing.
        centis = [round(i * 40000 / 5963) for i in range(len(gif_frames) + 1)]
        durations = [10 * (b - a) for a, b in zip(centis, centis[1:])]
        gif_frames[0].save(args.gif, save_all=True, append_images=gif_frames[1:],
                           duration=durations, loop=0, disposal=1, optimize=False)
    sweeps = None
    if "astraIdleSweeps" in core.symbols:
        sweeps = struct.unpack("<I", core.mu.mem_read(core.symbols["astraIdleSweeps"], 4))[0]
    core.call("cps1_close")
    expected_samples = args.frames * 22050 * 100 // 5963
    if samples != expected_samples or (not args.idle and not nonzero) or len(set(captures.values())) < 2:
        raise ValueError(f"Unexpected output: {samples}/{expected_samples} samples, {nonzero} audible frames")
    report = dict(rom_sha256=hashlib.sha256(args.rom.read_bytes()).hexdigest(),
                  elf_sha256=core.elf_hash,
                  inputs="idle" if args.idle else "coin/start/select/walk/attack/jump",
                  frames=args.frames, stereo_pairs=samples, audible_frames=nonzero, peak=peak,
                  captures=captures, serviced_calls=core.counts,
                  draw_period=args.draw_period, original_cpu=args.original_cpu, idle_sweeps=sweeps,
                  streams={k: v.hexdigest() for k, v in digests.items()},
                  end_of_frame_68k_pc=dict(pc_counts.most_common(16)))
    (args.out / "frames.crc").write_bytes(frame_checks)
    if args.compare_idle:
        idle = json.loads(args.compare_idle.read_text())
        if idle["inputs"] != "idle" or any(idle[k] != report[k] for k in ("rom_sha256", "elf_sha256", "frames")):
            raise ValueError("Reference must be an idle run of the same ROM, ELF and duration")
        if any(idle["captures"][str(f)] != digest for f, digest in captures.items() if f < 285):
            raise ValueError("Before the first input, the two runs already differ")
        changed = [f for f, digest in captures.items() if f >= 285 and idle["captures"][str(f)] != digest]
        if len(changed) < 2:
            raise ValueError("Controls did not produce distinct pictures")
        report["different_from_idle"] = changed
    (args.out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    if args.compare_cpu:
        original = json.loads(args.compare_cpu.read_text())
        if not original["original_cpu"] or any(original[k] != report[k] for k in
                    ("elf_sha256", "rom_sha256", "inputs", "draw_period", "frames")):
            raise ValueError("CPU reference must have matching binary, inputs and draw schedule")
        if sweeps is None or not sweeps:
            raise ValueError("Accelerated path did not execute")
        if report["streams"] != original["streams"]:
            baseline = (args.compare_cpu.parent / "frames.crc").read_bytes()
            for offset in range(0, len(baseline), 4):
                if baseline[offset:offset+4] != frame_checks[offset:offset+4]:
                    part = list(digests)[offset // 4 % 4]
                    raise ValueError(f"{part} diverged at frame {offset // 16}")
            raise ValueError("CPU stream digests differ")
        print(f"All video, PCM, work RAM and 68000 state hashes match after {sweeps} skipped loops")
    print(f"{samples} stereo pairs. {nonzero} audible frames. Peak {peak}.")


if __name__ == "__main__":
    main()
