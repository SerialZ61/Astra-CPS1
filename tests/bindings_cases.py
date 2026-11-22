"""Exercise the linked ARM button profiles on a disposable directory inside the project.

python tests/bindings_cases.py out/AstraCPS1.elf
"""
import argparse
import errno
import os
from pathlib import Path
import struct
import tempfile

from arm_smoke import ArmCore, REGS, STR, arm


class ProfileCore(ArmCore):
    def __init__(self, elf, root):
        super().__init__(elf, Path("unused"))
        self.root = root.resolve()
        self.fail_rename = False
        self.fail_flush = False
        self.errp = STR + 2048
        for name in ("__errno", "mkdir", "remove", "rename", "fwrite", "fsync", "fileno"):
            if name == "fileno" and name not in self.symbols:
                continue  # newlib may inline FILE's descriptor read
            address = self.symbols[name]
            self.patches[address & ~1] = name
            thunk = bytes.fromhex("00df7047") if address & 1 else bytes.fromhex("000000ef1eff2fe1")
            self.mu.mem_write(address & ~1, thunk)

    def path(self, address):
        name = self.cstr(address)
        assert name.startswith("sdmc:/")
        path = (self.root / name[6:]).resolve()
        assert self.root in path.parents
        return path

    def interrupt(self, mu, number, unused):
        pc = mu.reg_read(arm.UC_ARM_REG_PC)
        step = 2 if mu.reg_read(arm.UC_ARM_REG_CPSR) & 0x20 else 4
        name = self.patches.get(pc - step)
        a, b, c, d = mu.reg_read_batch(REGS)
        result = 0
        try:
            if name == "__errno":
                result = self.errp
            elif name == "mkdir":
                self.path(a).mkdir()
            elif name == "remove":
                self.path(a).unlink()
            elif name == "rename":
                old, new = self.path(a), self.path(b)
                if self.fail_rename and old.suffix == ".tmp":
                    raise OSError(errno.EIO, "simulated loss during final rename")
                if new.exists():
                    raise FileExistsError(errno.EEXIST, "FAT rename does not replace")
                old.rename(new)
            elif name in ("fopen", "fopen64"):
                mode = self.cstr(b)
                assert mode in ("wb", "rb")
                f = self.path(a).open(mode)
                result = self.alloc(256)  # newlib FILE flags may be read inline
                self.files[result] = f
                if mode == "wb": self.writer = result
            elif name == "fwrite":
                result = self.files[d].write(mu.mem_read(a, b * c)) // b
            elif name == "fflush":
                self.files[a].flush()
            elif name == "fileno":
                result = a
            elif name == "fsync":
                if self.fail_flush:
                    raise OSError(errno.EIO, "simulated media flush failure")
                os.fsync(self.files[self.writer].fileno())
            else:
                return super().interrupt(mu, number, unused)
        except OSError as e:
            mu.mem_write(self.errp, struct.pack("<I", e.errno))
            result = 0 if name in ("fopen", "fopen64") else -1
        mu.reg_write(arm.UC_ARM_REG_R0, result & 0xFFFFFFFF)


def check(elf, root):
    core = ProfileCore(elf, root)
    mu, call = core.mu, core.call
    buf, game, ext = STR + 64, STR + 256, STR + 320
    config = root / "3ds/AstraCPS1/config"

    def textarg(where, value):
        mu.mem_write(where, value.encode() + b"\0")
        return where

    def contents():
        return bytes(mu.mem_read(buf, 16))

    # Defaults and the exact libctru physical bits. Each direction on the
    # Circle Pad remains independent from its D-pad counterpart.
    keys = [0, 1, 10, 11, 9, 8, 3, 2, 6, 7, 5, 4, 30, 31, 29, 28]
    call("bindings_defaults", buf, 2)
    original = contents()
    assert original == bytes([7,8,13,14,15,16,6,5,1,2,3,4,1,2,3,4])
    assert call("bindings_buttons", buf, 1 << keys[2]) == (64 | 128)
    assert call("bindings_pressed", buf, 14, 1 << keys[3]) == 1
    assert call("bindings_pressed", buf, 14, 0) == 0
    independent = bytearray(original)
    independent[8], independent[12] = 5, 6
    mu.mem_write(buf, bytes(independent))
    assert call("bindings_buttons", buf, (1 << keys[8])) == 1
    assert call("bindings_buttons", buf, (1 << keys[12])) == 2
    assert call("bindings_buttons", buf, (1 << keys[8]) | (1 << keys[12])) == 3
    assert core.cstr(call("bindings_key_name", 15)) == "Pad droite"
    assert core.cstr(call("bindings_action_name", 7, 2, 1)) == "Attaque"
    assert core.cstr(call("bindings_action_name", 12, 6, 0)) == "Pied fort"
    call("bindings_defaults", buf, 3)
    assert contents()[2] == 9
    call("bindings_defaults", buf, 6)
    fighter = contents()
    assert fighter[:6] == bytes([11,10,8,7,9,12])
    for index, shift in enumerate([10,9,7,6,8,11,1,0,2,3,4,5,2,3,4,5]):
        assert call("bindings_buttons", buf, 1 << keys[index]) == 1 << shift
    assert not call("bindings_allowed", 12, 2)
    assert call("bindings_allowed", 12, 6)
    assert not call("bindings_allowed", 0xFFFFFFFF, 6)
    print("2-, 3- and 6-action defaults; all 16 physical keys; shortcut edges: passed")

    # Writes never escape the temporary host directory. The emulator core
    # and its FNV record checks are the ARM project binary throughout.
    call("bindings_load", buf, textarg(game, "ffight"), 2)
    assert contents() == original and not config.exists()
    mu.mem_write(buf, bytes([8,7]) + original[2:])
    second = contents()
    assert call("bindings_save", buf, game)
    assert call("bindings_load", buf, game, 2) and contents() == second
    call("bindings_defaults", buf, 6)
    assert call("bindings_save", buf, textarg(game, "sf2"))
    assert call("bindings_load", buf, textarg(game, "ffight"), 2) and contents() == second
    mu.mem_write(buf, fighter)
    assert call("bindings_save", buf, textarg(game, "sf2"))
    assert call("bindings_load", buf, game, 6) and contents() == fighter
    main = config / "ffight.map"
    backup = config / "ffight.map.bak"
    stage = config / "ffight.map.tmp"
    mu.mem_write(buf, original)
    assert call("bindings_save", buf, textarg(game, "ffight")) and backup.exists()
    damaged = bytearray(main.read_bytes())
    damaged[-1] ^= 4
    main.write_bytes(damaged)
    assert call("bindings_load", buf, game, 2) and contents() == second
    print("independent game profiles, corruption at unchanged length, backup recovery: passed")

    # Interrupt the commit after rotating the previous valid main to backup.
    mu.mem_write(buf, original)
    assert call("bindings_save", buf, game)
    core.fail_flush = True
    mu.mem_write(buf, second)
    assert not call("bindings_save", buf, game)
    core.fail_flush = False
    assert call("bindings_load", buf, game, 2) and contents() == original
    core.fail_rename = True
    mu.mem_write(buf, second)
    assert not call("bindings_save", buf, game)
    core.fail_rename = False
    assert not main.exists() and backup.exists() and stage.exists()
    assert call("bindings_load", buf, game, 2) and contents() == original
    # First write interruption can leave only the verified temporary record.
    backup.unlink()
    assert call("bindings_load", buf, game, 2) and contents() == second
    mu.mem_write(buf, original)
    assert call("bindings_save", buf, game)
    assert main.exists() and backup.exists() and not stage.exists()
    assert call("bindings_load", buf, game, 2) and contents() == original
    print("failed media flush, interrupted final rename, orphan temporary record, subsequent write: passed")

    # Validate record type, game path, unknown action and truncated size.
    assert call("profile_write", game, textarg(ext, "dip"), buf, 3)
    mu.mem_write(buf, bytes(3))
    assert call("profile_read", game, ext, buf, 3) and contents()[:3] == original[:3]
    assert not call("profile_write", textarg(game, "../bad"), ext, buf, 3)
    mu.mem_write(buf, b"\xff" + original[1:])
    assert not call("bindings_save", buf, textarg(game, "ffight"))
    assert call("profile_write", game, textarg(ext, "map"), buf, 16)
    assert not call("bindings_load", buf, game, 2) and contents() == original
    mu.mem_write(buf, b"ZZZZ")
    assert not call("profile_read", game, textarg(ext, "dip"), buf, 4)
    assert contents()[:4] == b"ZZZZ"
    print("DIP and button coexistence, invalid paths, invalid actions and sizes: passed")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path)
    args = parser.parse_args()
    project = Path(__file__).resolve().parent.parent
    qa = project / "out" / "qa-bindings"
    qa.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=qa) as temp:
        check(args.elf, Path(temp))
