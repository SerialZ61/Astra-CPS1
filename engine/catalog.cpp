#include "cps1.h"
#include "burnint.h"
#include "compat/api.h"

const BurnDriver *astra_driver(int index);
namespace {
struct Settings { const char *title; int buttons; int mhz; };
const Settings games[] = {
#define ASTRA_GAME(driver, title, buttons, mhz) {title, buttons, mhz},
#include "drivers.inc"
#undef ASTRA_GAME
};
struct Extra { const char *id, *parent, *title; int buttons; };
const Extra extras[] = {
#define EXTRA(symbol,id,parent,title,buttons) {id,parent,title,buttons},
#include "compat/sets.inc"
#undef EXTRA
};
constexpr int nativeCount = sizeof(games) / sizeof(games[0]);
}

int astraExtraIndex(int game) { return game >= nativeCount && game < cps1_game_count() ? game-nativeCount : -1; }
int cps1_game_count() { return nativeCount + sizeof(extras)/sizeof(extras[0]); }

const char *cps1_game_id(int game)
{
    int extra = astraExtraIndex(game);
    if (extra >= 0) return extras[extra].id;
    const BurnDriver *driver = astra_driver(game);
    return driver ? driver->szShortName : NULL;
}

const char *cps1_game_parent(int game)
{
    int extra = astraExtraIndex(game);
    if (extra >= 0) return extras[extra].parent;
    const BurnDriver *driver = astra_driver(game);
    return driver ? driver->szParent : NULL;
}

const char *cps1_game_title(int game)
{
    int extra = astraExtraIndex(game);
    if (extra >= 0) return extras[extra].title;
    return game >= 0 && game < cps1_game_count() ? games[game].title : NULL;
}

int cps1_game_buttons(int game)
{
    int extra = astraExtraIndex(game);
    if (extra >= 0) return extras[extra].buttons;
    return game >= 0 && game < cps1_game_count() ? games[game].buttons : 0;
}

int cps1_game_vertical(int game)
{
    int extra = astraExtraIndex(game);
    if (extra >= 0) return astra_extra_api.vertical(extra);
    const BurnDriver *driver = astra_driver(game);
    return driver && (driver->flags & BDF_ORIENTATION_VERTICAL);
}

int astra_game_clock(int game) { return games[game].mhz * 1000000; }

int cps1_game_chip(int game, unsigned index, const char **name, uint32_t *bytes, uint32_t *crc)
{
    int extra = astraExtraIndex(game);
    if (extra >= 0) return astra_extra_api.chip(extra, index, name, bytes, crc);
    const BurnDriver *driver = astra_driver(game);
    BurnRomInfo chip = {};
    char *label = NULL;
    if (!driver || driver->GetRomInfo(&chip, index) || driver->GetRomName(&label, index, 0))
        return 1;
    if (name) *name = label;
    if (bytes) *bytes = chip.nLen;
    if (crc) *crc = chip.nCrc;
    return 0;
}
