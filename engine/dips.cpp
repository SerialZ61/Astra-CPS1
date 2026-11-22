#include "cps1.h"
#include "cps.h"
#include <vector>
int astraInputInfo(BurnInputInfo *, unsigned);
int astraDipInfo(BurnDIPInfo *, unsigned);

namespace {
struct Group {
    const char *label;
    int first, count;
    unsigned char *bank, mask, defaults;
};
std::vector<Group> groups;

const Group *lookup(int group)
{
    return group >= 0 && group < cps1_dip_count() ? &groups[group] : NULL;
}

bool option(const Group *group, int index, BurnDIPInfo &out)
{
    return group && index >= 0 && index < group->count &&
           !astraDipInfo(&out, group->first + index);
}

int findValue(const Group *group, unsigned char value)
{
    BurnDIPInfo row;
    if (group) {
        for (int i = 0; option(group, i, row); ++i)
            if ((value & group->mask) == row.nSetting) return i;
    }
    return -1;
}
}

void astraDipsOpen()
{
    groups.clear();
    Cpi01A = Cpi01C = Cpi01E = 0;
    BurnDIPInfo row;
    int offset = 0;
    for (int i = 0; !astraDipInfo(&row, i); ++i) {
        if (row.nFlags == 0xf0) offset = row.nInput;
        if (row.nFlags == 0xff) {
            BurnInputInfo in;
            if (!astraInputInfo(&in, row.nInput + offset) && in.nType == BIT_DIPSWITCH)
                *in.pVal = (*in.pVal & ~row.nMask) | (row.nSetting & row.nMask);
        } else if (row.nFlags == 0xfe && row.nSetting && row.szText) {
            BurnDIPInfo first;
            BurnInputInfo in;
            if (astraDipInfo(&first, i + 1) || first.nFlags != 1 ||
                astraInputInfo(&in, first.nInput + offset) || in.nType != BIT_DIPSWITCH) continue;
            groups.push_back({row.szText, i + 1, row.nSetting, in.pVal, first.nMask, *in.pVal});
        }
    }
}

int cps1_dip_count() { return cps1_current_game() >= 0 ? (int)groups.size() : 0; }
const char *cps1_dip_name(int g) { const Group *p = lookup(g); return p ? p->label : NULL; }
int cps1_dip_option_count(int g) { const Group *p = lookup(g); return p ? p->count : 0; }

const char *cps1_dip_option_name(int g, int i)
{
    BurnDIPInfo row;
    return option(lookup(g), i, row) ? row.szText : NULL;
}

int cps1_dip_get(int g)
{
    const Group *p = lookup(g);
    return p ? findValue(p, *p->bank) : -1;
}

int cps1_dip_default(int g)
{
    const Group *p = lookup(g);
    return p ? findValue(p, p->defaults) : -1;
}

int cps1_dip_set(int g, int i)
{
    const Group *p = lookup(g);
    BurnDIPInfo row;
    if (!option(p, i, row)) return 1;
    *p->bank = (*p->bank & ~p->mask) | (row.nSetting & p->mask);
    return 0;
}

void cps1_dip_defaults()
{
    if (cps1_current_game() >= 0)
        for (const Group &g : groups) *g.bank = g.defaults;
}
