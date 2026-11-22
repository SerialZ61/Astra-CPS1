#include "ui_language.h"
#include "cps1.h"
#include "cps.h"
#include "romzip.h"
#include "compat/api.h"
int astraExtraIndex(int game);
int astraInputInfo(BurnInputInfo *, unsigned);

extern "C" void astraIdleEnable(int enabled);
extern "C" void astraTaskEnable(const uint16_t *rom, unsigned bytes, int layout);
void astraDipsOpen();
void AstraSf2StripsOpen(bool enable);
bool AstraCacheAllPlanes;
extern "C" void AstraSf2Music(int enabled);

namespace {
int current = -1;
bool useExtra;
unsigned short *dial;
unsigned samplePhase;
int16_t mutedAudio[2 * CPS1_MAX_SAMPLES];
char message[256];
const char *const tags[] = {
    "p1 coin", "p1 start", "p1 up", "p1 down", "p1 left", "p1 right",
    "p1 fire 1", "p1 fire 2", "p1 fire 3", "p1 fire 4", "p1 fire 5", "p1 fire 6"
};
unsigned char *inputs[sizeof(tags) / sizeof(tags[0])];

unsigned rgb565(int red, int green, int blue, int)
{
    return ((red & 0xf8) << 8) | ((green & 0xfc) << 3) | (blue >> 3);
}

void bindInputs()
{
    memset(inputs, 0, sizeof(inputs));
    dial = NULL;
    BurnInputInfo input;
    for (unsigned i = 0; !astraInputInfo(&input, i); ++i) {
        if (!input.szInfo) continue;
        if (input.nType == BIT_ANALOG_REL && !strcmp(input.szInfo, "p1 z-axis")) dial = input.pShortVal;
        if (input.nType != BIT_DIGITAL) continue;
        for (unsigned j = 0; j < sizeof(inputs) / sizeof(inputs[0]); ++j)
            if (!strcmp(input.szInfo, tags[j])) inputs[j] = input.pVal;
    }
}

}

int astraInputInfo(BurnInputInfo *in, unsigned row)
{
    return useExtra ? astra_extra_api.input(in, row) : BurnDrvGetInputInfo(in, row);
}
int astraDipInfo(BurnDIPInfo *in, unsigned row)
{
    return useExtra ? astra_extra_api.dip(in, row) : BurnDrvGetDIPInfo(in, row);
}

int cps1_open_game(int requested, const char *path)
{
    cps1_close();
    message[0] = 0;
    int game = romzip_prepare(path, requested, message, sizeof(message));
    if (game < 0) return 1;
    int extra = astraExtraIndex(game);
    useExtra = extra >= 0;
    samplePhase = 0;
    if (useExtra) {
        int result = astra_extra_api.open(extra, romzip_load, CPS1_SAMPLE_RATE, CPS1_MAX_SAMPLES);
        romzip_release();
        if (result) {
            astra_extra_api.close();
            useExtra = false;
            snprintf(message, sizeof(message), ui_text("Echec du pilote %.96s.", "Could not start driver %.96s."), cps1_game_title(game));
            return 1;
        }
        current = game;
        bindInputs();
        astraDipsOpen();
        return 0;
    }
    BurnLibInit();
    nBurnDrvSelect = game;
    bForce60Hz = false;
    nBurnSoundRate = CPS1_SAMPLE_RATE;
    nBurnSoundLen = CPS1_MAX_SAMPLES;
    nBurnBpp = 2;
    nBurnPitch = CPS1_WIDTH * sizeof(uint16_t);
    BurnHighCol = rgb565;
    BurnExtLoadRom = romzip_load;
    nCurrentFrame = 0;

    // This Z80 optimization recognizes only the verified sound program.
    unsigned z80Crc = 0;
    uint32_t crc;
    for (unsigned i = 0; !cps1_game_chip(game, i, NULL, NULL, &crc); ++i) {
        if (crc == 0xb8367eb5u || crc == 0xa4823a1bu || crc == 0x08f6b60eu) z80Crc = crc;
    }
    Z80SetIdleRomCrc(z80Crc);
    bool ready = BurnDrvInit() == 0;
    romzip_release();
    BurnExtLoadRom = NULL;
    if (!ready) {
        BurnDrvExit();
        BurnLibExit();
        Z80SetIdleRomCrc(0);
        snprintf(message, sizeof(message), ui_text("Echec du pilote %.96s.", "Could not start driver %.96s."), cps1_game_title(game));
        return 1;
    }
    current = game;
    bindInputs();
    astraDipsOpen();
    const char *id = cps1_game_id(game);
    bool sf2 = !strcmp(id, "sf2");
    bool champion = !strncmp(id, "sf2ce", 5);
    bool zero = !strcmp(id, "sfzch"), dynasty = !strncmp(id, "dynwar", 6);
    AstraCacheAllPlanes = Knights || Ghouls || Msword || champion || zero || dynasty;
    AstraSf2StripsOpen(sf2 || AstraCacheAllPlanes);
    AstraSf2Music(sf2 || AstraCacheAllPlanes);
    int idle = sf2 ? 2 : !strcmp(id, "ffight");
    if (!strcmp(id, "knights")) idle = 3;
    if (!strcmp(id, "daimakai")) idle = 4;
    astraIdleEnable(idle);
    if (champion || zero || dynasty)
        astraTaskEnable((const uint16_t *)CpsRom, nCpsRomLen, champion ? 0 : zero ? 1 : 2);
    BurnRecalcPal();
    return 0;
}

int cps1_open(const char *path) { return cps1_open_game(-1, path); }
int cps1_missing_samples() { return current >= 0 && romzip_missing_samples(); }
const char *cps1_error() { return message; }
int cps1_current_game() { return current; }
void cps1_reset()
{
    if (useExtra) astra_extra_api.reset(true);
    else if (current >= 0) CpsReset = 1;
}

int cps1_frame(uint32_t buttons, uint16_t *video, int16_t *audio)
{
    if (current < 0) return -1;
    for (unsigned i = 0; i < sizeof(inputs) / sizeof(inputs[0]); ++i)
        if (inputs[i]) *inputs[i] = (buttons >> i) & 1;
    if (dial) *dial = ((buttons & CPS1_BUTTON3) ? 0x600 : 0) - ((buttons & CPS1_JUMP) ? 0x600 : 0);
    samplePhase += CPS1_SAMPLE_RATE * CPS1_FPS_DEN;
    nBurnSoundLen = samplePhase / CPS1_FPS_NUM;
    samplePhase %= CPS1_FPS_NUM;
    pBurnSoundOut = audio ? audio : mutedAudio;
    if (useExtra) {
        int result = astra_extra_api.frame(video, pBurnSoundOut, nBurnSoundLen);
        astra_extra_api.reset(false);
        if (result) snprintf(message, sizeof(message), ui_text("Erreur pendant l'emulation.", "Emulation error."));
        return result ? -1 : nBurnSoundLen;
    }
    pBurnDraw = reinterpret_cast<unsigned char *>(video);
    int result = BurnDrvFrame();
    CpsReset = 0;
    if (result != 0) {
        snprintf(message, sizeof(message), ui_text("Erreur pendant l'emulation.", "Emulation error."));
        return -1;
    }
    // CPS-A VIDEOCTRL bit 15 reverses both scan directions on the monitor.
    // Use the latched register from this picture, as games can change it at runtime.
    if (video && (CpsSaveReg[0][0x23] & 0x80)) {
        uint16_t *left = video;
        uint16_t *right = video + CPS1_WIDTH * CPS1_HEIGHT - 1;
        while (left < right) {
            uint16_t pixel = *left;
            *left++ = *right;
            *right-- = pixel;
        }
    }
    ++nCurrentFrame;
    return nBurnSoundLen;
}

void cps1_close()
{
    if (useExtra) {
        astra_extra_api.close();
        useExtra = false;
        current = -1;
    } else if (current >= 0) {
        AstraSf2StripsOpen(false);
        AstraSf2Music(false);
        astraIdleEnable(0);
        BurnDrvExit();
        BurnLibExit();
        current = -1;
    }
    Z80SetIdleRomCrc(0);
    CpsReset = 0;
    pBurnSoundOut = NULL;
    pBurnDraw = NULL;
}
