#include "cyclone.h"
#include <stdint.h>
#include <string.h>

extern unsigned char *CpsRamFF;
extern "C" {
extern void (*AstraCycloneJumpTable[])(void);
void AstraCycloneDbra(void);
void AstraIdleBranch(void);
void AstraSf2IdleBranch(void);
void AstraKnightsBranch(void);
void AstraDaimakaiBranch(void);
void AstraTaskBranch(void);
uint32_t astraTaskPc;
uint32_t astraIdleSweeps; // Number of individual waiting task slots accelerated.

void astraIdleEnable(int enabled)
{
    // The loader has identified the exact revision by the program chip CRCs.
    // The handler table belongs to Cyclone. No instruction in the ROM changes.
    AstraCycloneJumpTable[0x51c9] = enabled == 4 ? AstraDaimakaiBranch : AstraCycloneDbra;
    AstraCycloneJumpTable[0x51c8] = enabled == 3 ? AstraKnightsBranch : enabled == 2 ? AstraSf2IdleBranch :
        enabled == 1 ? AstraIdleBranch : AstraCycloneDbra;
    astraIdleSweeps = 0;
}

}
namespace {
// Both audited programs use this nine-instruction polling loop. Their
// work tables and record sizes differ; the opcode dispatch selects the revision.
template<unsigned stride, unsigned tableEnd, unsigned wakeFlag, bool copyState = true, int slot_cycles = 98>
int scanWaitingTasks(Cyclone *cpu, int remaining)
{
    // At the loop DBRA, A0 points to the next entry in a 16-slot
    // table. A waiting entry has a state byte <4. Visiting such a slot
    // reads only work RAM. A runnable entry calls arbitrary game code and
    // must always take Cyclone's original path.
    //
    // One DBRA-to-DBRA waiting iteration in this Cyclone build costs:
    // DBRA(taken) 10, MOVE to SR 16, TST.B 12, BNE(untaken) 8,
    // MOVE.B 12, CMPI.B 8, BCC(untaken) 8, MOVE to SR 16, LEA 8.
    const unsigned count = cpu->d[0] & 0xffff;
    if (remaining < slot_cycles || !count || count > 15 || cpu->irq ||
        cpu->srh != 0x20 || (cpu->a[5] & 0xffffff) != 0xff8000 ||
        (cpu->a[0] & 0xffffff) != tableEnd - count * stride ||
        CpsRamFF[wakeFlag ^ 1])
        return remaining;

    unsigned consumed = 0, state = 0;
    const unsigned offset = cpu->a[0] & 0xffff;
    while (consumed < count && remaining >= slot_cycles) {
        unsigned next_state = CpsRamFF[(offset + consumed * stride) ^ 1];
        if (next_state >= 4) break;
        state = next_state;
        ++consumed;
        remaining -= slot_cycles;
    }
    if (consumed) {
        // Both MOVE-to-SR instructions leave supervisor mode active, with
        // SR=0x2000 again at DBRA. Only D0.W, D1.B and A0 change between
        // the two endpoints. The final (non-taken) DBRA runs normally.
        cpu->d[0] -= consumed;
        if (copyState) cpu->d[1] = (cpu->d[1] & ~0xffu) | state;
        cpu->a[0] += consumed * stride;
        astraIdleSweeps += consumed;
    }
    return remaining;
}
}

namespace {
enum TaskLayout { Champion, Zero, Dynasty };
TaskLayout layout;
}

extern "C" void astraTaskEnable(const uint16_t *rom, unsigned bytes, int kind)
{
    // Match the complete inactive-task path, including table setup and
    // backward branch. Some regional revisions place it at different PCs.
    layout = static_cast<TaskLayout>(kind);
    unsigned short wake = layout == Champion ? 0x820e : layout == Zero ? 0x849e : 0x0104;
    unsigned short table = layout == Champion ? 0x8000 : layout == Zero ? 0x8290 : 0x0004;
    unsigned short prefix[] = {0x422d,wake,0x41ed,table,0x303c,15,0x46fc,0x2600,0x4a2d,wake,0x66ea};
    const unsigned short displaced[] = {0x1228,0};
    const unsigned short direct[] = {0x1210};
    const unsigned short *load = layout == Zero ? direct : displaced;
    unsigned loadBytes = layout == Zero ? sizeof(direct) : sizeof(displaced);
    unsigned short tail[] = {0x0c01,4,(uint16_t)(layout == Dynasty ? 0x6418 : 0x6414),
        0x46fc,0x2000,0x41e8,(uint16_t)(layout == Dynasty ? 16 : 32),0x51c8,
        (uint16_t)(layout == Zero ? 0xffe4 : 0xffe2)};
    unsigned length = sizeof(prefix) + loadBytes + sizeof(tail);
    unsigned limit = bytes < 0x4000 ? bytes : 0x4000;
    for (unsigned offset = 0; offset + length <= limit; offset += 2) {
        const uint16_t *p = rom + offset / 2;
        if (memcmp(p, prefix, sizeof(prefix))) continue;
        p += sizeof(prefix) / 2;
        if (memcmp(p, load, loadBytes) || memcmp(p + loadBytes / 2, tail, sizeof(tail))) continue;
        astraTaskPc = offset + length - 2; // Cyclone already fetched DBRA.
        AstraCycloneJumpTable[0x51c8] = AstraTaskBranch;
        return;
    }
}

extern "C" int astraTaskBudget(Cyclone *cpu, int remaining)
{
    if (layout == Champion) return scanWaitingTasks<32, 0xff0200, 0x020e>(cpu, remaining);
    if (layout == Dynasty) return scanWaitingTasks<16, 0xff8104, 0x8104>(cpu, remaining);
    // MOVE.B (A0),D1 costs four fewer cycles than MOVE.B 0(A0),D1.
    return scanWaitingTasks<32, 0xff0490, 0x049e, true, 94>(cpu, remaining);
}

extern "C" int astraIdleBudget(Cyclone *cpu, int remaining)
{
    return scanWaitingTasks<16, 0xff1100, 0x1100>(cpu, remaining);
}

extern "C" int astraSf2IdleBudget(Cyclone *cpu, int remaining)
{
    return scanWaitingTasks<32, 0xff0200, 0x020e>(cpu, remaining);
}

extern "C" int astraKnightsIdleBudget(Cyclone *cpu, int remaining)
{
    // This revision compares the task status in memory, without writing D1.
    return scanWaitingTasks<16, 0xff814c, 0x814c, false>(cpu, remaining);
}

extern "C" int astraDaimakaiIdleBudget(Cyclone *cpu, int remaining)
{
    // DBRA D1, two MOVE-to-SR, MOVE.B, ANDI.B, BNE.W and LEA: 82 cycles.
    const unsigned count = cpu->d[1] & 0xffff, next = cpu->a[2] & 0xffffff;
    if (cpu->irq || cpu->srh != 0x20 || (cpu->a[5] & 0xffffff) != 0xff8000 ||
        count > 15 || next != 0xff0106 - count*16) return remaining;
    unsigned used = 0;
    while (used < count && remaining >= 82 && !(CpsRamFF[((next & 0xffff) + used*16)^1] & 3)) {
        remaining -= 82;
        ++used;
    }
    if (used) {
        cpu->d[1] -= used;
        cpu->d[3] &= ~0xffu;
        cpu->a[2] += used*16;
        astraIdleSweeps += used;
    }
    return remaining;
}
