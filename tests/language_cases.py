"""Drive the linked ARM language menus, settings and SD profile with simulated HID."""
import argparse
import struct
import tempfile
from pathlib import Path

from arm_smoke import STR, REGS, arm
from bindings_cases import ProfileCore

A, B, Y, DOWN, TOUCH = 1, 2, 1 << 11, 1 << 7, 1 << 20


class MenuCore(ProfileCore):
    def __init__(self, elf, folder, system):
        super().__init__(elf, folder)
        self.system = system
        self.keys = []
        self.point = (20, 161)
        self.output = []
        services = ("aptMainLoop hidScanInput hidKeysDown hidKeysDownRepeat "
                    "hidSetRepeatParameters hidTouchRead gspWaitForEvent "
                    "display_service display_flush_console cfguInit cfguExit CFGU_GetSystemLanguage")
        for name in services.split():
            entry = self.symbols[name]
            self.patches[entry & ~1] = name
            self.mu.mem_write(entry & ~1, bytes.fromhex("00df7047" if entry & 1 else "000000ef1eff2fe1"))

    def interrupt(self, mu, number, unused):
        pc = mu.reg_read(arm.UC_ARM_REG_PC)
        name = self.patches.get(pc - (2 if mu.reg_read(arm.UC_ARM_REG_CPSR) & 0x20 else 4))
        arg = mu.reg_read(REGS[0])
        result = 0
        if name in ("puts", "printf"):
            self.output.append(self.cstr(arg))
        elif name == "CFGU_GetSystemLanguage":
            mu.mem_write(arg, bytes([self.system]))
        elif name == "aptMainLoop":
            result = 1
        elif name == "hidKeysDown":
            if not self.keys:
                raise AssertionError("Menu requested an unexpected input")
            result = self.keys.pop(0)
        elif name == "hidTouchRead":
            mu.mem_write(arg, struct.pack("<HH", *self.point))
        elif name not in ("hidScanInput", "hidKeysDownRepeat", "hidSetRepeatParameters",
                          "gspWaitForEvent", "display_service", "display_flush_console",
                          "cfguInit", "cfguExit"):
            return super().interrupt(mu, number, unused)
        mu.reg_write(REGS[0], result)

    @property
    def english(self):
        return self.mu.mem_read(self.symbols["ui_english"], 1)[0]

    def menu(self, function, keys, *args):
        self.keys = list(keys)
        self.output.clear()
        self.call(function, *args)
        assert not self.keys
        return "\n".join(self.output)


def check(elf, folder):
    library, bindings = STR + 320, STR + 512
    fr = MenuCore(elf, folder, system=2)
    fr.call("language_load")
    assert not fr.english
    # Empty library, open using Y, move from French to English, save, return.
    text = fr.menu("menu_choose", [Y, DOWN, A, B], library, 0)
    assert fr.english and "JEUX SUR LA SD" in text and "SD CARD GAMES" in text
    assert fr.cstr(fr.call("bindings_action_name", 12, 6, 0)) == "Heavy kick"
    restart = MenuCore(elf, folder, system=2)
    restart.call("language_load")
    assert restart.english
    # In-game settings, change back to French and cancel out of settings.
    restart.call("bindings_defaults", bindings, 2)
    text = restart.menu("settings_edit", [DOWN, DOWN, A, 1 << 6, A, B], 0, bindings)
    assert not restart.english
    assert restart.cstr(restart.call("bindings_action_name", 7, 2, 1)) == "Attaque"
    # Touch the language row even when the library is empty. Cancelling must
    # preserve the previous selection on the card and in memory.
    restart.menu("menu_choose", [TOUCH, DOWN, B, B], library, 0)
    again = MenuCore(elf, folder, system=1)
    again.call("language_load")
    assert not again.english
    # A failed SD write leaves the active choice and prior saved value valid.
    again.fail_flush = True
    assert not again.call("language_save", 1)
    assert not again.english
    print("Library button/touch, in-game menu, button labels, persistence, cancellation and failed save passed.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory() as temp:
        check(args.elf, Path(temp))
    with tempfile.TemporaryDirectory() as temp:
        for system, expected in [(0, 1), (1, 1), (2, 0), (5, 1)]:
            core = MenuCore(args.elf, Path(temp), system)
            core.call("language_load")
            assert core.english == expected
    print("First launch follows French system locale or falls back to English.")
