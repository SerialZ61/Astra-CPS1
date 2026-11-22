"""Run the linked ARM audio queue against a simulated, continuously draining DSP."""
import array
import math
import struct
import sys
from collections import deque
from pathlib import Path
from arm_smoke import ArmCore, BMP, STR, PCM, REGS, arm
from compare_releases import cabinet_player

RATE = 22050
CLOCK = 16756991 * 16


class AudioCore(ArmCore):
    def __init__(self, elf, archive=None):
        super().__init__(elf, archive or Path("unused"))
        self.time = 0
        self.paused = True
        self.queue = deque()
        self.chunks = []
        self.missing = 0
        services = ("ndspInit linearAlloc ndspSetOutputMode ndspChnReset ndspChnSetFormat "
                    "ndspChnSetInterp ndspChnSetRate ndspChnSetMix ndspChnSetPaused "
                    "ndspChnWaveBufClear ndspChnWaveBufAdd DSP_FlushDataCache svcGetSystemTick svcSleepThread")
        self.services = services.split()
        for name in self.services:
            addr = self.symbols[name]
            self.patches[addr & ~1] = name
            code = "00df7047" if addr & 1 else "000000ef1eff2fe1"
            self.mu.mem_write(addr & ~1, bytes.fromhex(code))

    def interrupt(self, mu, number, unused):
        pc = mu.reg_read(arm.UC_ARM_REG_PC)
        step = 2 if mu.reg_read(arm.UC_ARM_REG_CPSR) & 0x20 else 4
        name = self.patches.get(pc-step)
        if name not in self.services:
            return super().interrupt(mu, number, unused)
        a, b, _, _ = mu.reg_read_batch(REGS)
        result = 0
        if name == "linearAlloc":
            result = self.alloc(a, 32)
        elif name == "svcSleepThread":
            self.advance(self.time + max(1, round(a*RATE/1e9)))
        elif name == "svcGetSystemTick":
            ticks = self.time * CLOCK // RATE
            result = ticks & 0xffffffff
            mu.reg_write(arm.UC_ARM_REG_R1, ticks >> 32)
        elif name == "ndspChnSetPaused":
            self.paused = bool(b)
        elif name == "ndspChnWaveBufClear":
            self.queue.clear()
        elif name == "ndspChnWaveBufAdd":
            addr, pairs = struct.unpack("<II", mu.mem_read(b, 8))
            assert 0 < pairs <= 1110
            self.chunks.append(bytes(mu.mem_read(addr, pairs*4)))
            mu.mem_write(b+17, bytes([1]))
            self.queue.append([b, pairs])
        mu.reg_write(arm.UC_ARM_REG_R0, result)

    def advance(self, moment):
        due = moment - self.time
        self.time = moment
        while due and not self.paused and self.queue:
            wave = self.queue[0]
            count = min(due, wave[1])
            due -= count
            wave[1] -= count
            self.mu.mem_write(wave[0]+17, bytes([2 if wave[1] else 3]))
            if not wave[1]:
                self.queue.popleft()
        if not self.paused:
            self.missing += due


def check(elf, fps, continuous):
    c = AudioCore(elf)
    assert c.call("sound_open")
    position = 0
    for frame in range(350):
        end = (frame+1)*RATE*100//5963
        samples = array.array("h")
        for i in range(position, end):
            left = round(12000*math.sin(2*math.pi*440*i/RATE))
            samples.extend((left, -left))
        c.advance(round((frame+1)*RATE/fps))
        c.mu.mem_write(PCM, samples.tobytes())
        c.call("sound_submit", PCM, end-position, int(continuous))
        if not continuous or fps == 59.63:
            assert c.chunks[-1] == samples.tobytes()
        position = end
    assert not c.call("sound_drops")
    empty = c.call("sound_underruns")
    if continuous:
        data = array.array("h", b"".join(c.chunks[60:]))
        left = data[::2]
        assert all(a == -b for a, b in zip(left, data[1::2]))
        crossings = sum(a <= 0 < b for a,b in zip(left,left[1:]))
        hz = crossings*RATE/len(left)
        assert abs(hz-440) < 3, hz
        length = len(left)/((350-60)*RATE/fps)
        assert .99 < length < 1.01, length
        assert empty == 0 and c.missing == 0
        print(f"{fps:.2f} Hz: {hz:.1f} Hz, duration {length:.4f}, underruns {empty}")
    else:
        assert empty > 1
        print(f"{fps:.2f} Hz, original PCM: {empty} underruns")


def check_cadence(elf, mode, costs, archive=None):
    c = AudioCore(elf, archive)
    if archive:
        assert not c.call("cps1_open", STR)
    assert c.call("sound_open")
    period = CLOCK * 100 // 5963
    pacer = c.alloc(64, 8)
    c.call("pacing_reset", pacer, period)
    now = deadline = period
    original = bytes(370 * 4)
    c.mu.mem_write(PCM, original)
    drawn = []
    # Use the ARM scheduler and audio path together, alternating costly
    # drawings and cheaper logic-only ticks, as in the user's measurements.
    for frame in range(1800 if archive else 1000):
        c.advance(now * RATE // CLOCK)
        before = c.time
        c.call("sound_wait_for_slot")
        now += (c.time - before) * CLOCK // RATE
        cost_draw, cost_plain = costs[(frame // 100) % len(costs)]
        draw = c.call("pacing_wants_draw", pacer, mode, now & 0xffffffff, now >> 32)
        elapsed = round((cost_draw if draw else cost_plain) * CLOCK / 1000)
        c.call("pacing_record", pacer, draw, elapsed)
        if draw: drawn.append(now)
        c.advance((now + elapsed) * RATE // CLOCK)
        pairs = (frame+1)*RATE*100//5963 - frame*RATE*100//5963
        if archive:
            assert c.call("cps1_frame", cabinet_player(frame), BMP if draw else 0, PCM) == pairs
        c.call("sound_submit", PCM, pairs, 1)
        deadline += period
        now += elapsed
        if now < deadline:
            if c.call("sound_needs_refill"):
                deadline -= min(deadline-now, period//8)
            now = deadline
        elif now - deadline > 2*period:
            deadline = now
    assert c.call("sound_underruns") == 0 and not c.missing, (mode, costs, c.missing)
    assert c.call("sound_drops") == 0
    fps = len(drawn)*CLOCK/(now-period)
    if mode == 1:
        assert fps < 30.1
        if max(d for d, _ in costs) < 16:
            assert 29.5 <= fps <= 30.1
    if archive:
        assert any(any(chunk) for chunk in c.chunks[950:])
        c.call("cps1_close")
    print(f"{archive or 'synthetic'}, mode {mode}, costs {costs}: draw {fps:.1f}/s, no missing audio")


if __name__ == "__main__":
    elf = Path(sys.argv[1])
    for rate in (59.63, 51.6, 47.2, 41.2, 33.2, 25.2):
        check(elf, rate, True)
    check(elf, 41.2, False)
    for mode in (0, 1):
        for costs in ([(12, 7.7)], [(23, 10.5)], [(37.8, 22.1)],
                      [(12, 7.7), (27.5, 15), (37.8, 22.1), (17, 8)]):
            check_cadence(elf, mode, costs)
    for archive in map(Path, sys.argv[2:]):
        check_cadence(elf, 0, [(12, 7.7), (27.5, 15), (37.8, 22.1), (17, 8)], archive)
