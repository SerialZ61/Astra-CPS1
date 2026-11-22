#include "cps.h"
#include "api.h"

char g_save_dir[1024] = "sdmc:/3ds/AstraCPS1";
char g_system_dir[1024] = "sdmc:/3ds/AstraCPS1";

static int vertical(int index)
{
    unsigned saved = nBurnDrvActive;
    nBurnDrvActive = index;
    bool value = BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL;
    nBurnDrvActive = saved;
    return value;
}
static int chip(int index, unsigned number, const char **name, uint32_t *size, uint32_t *crc)
{
    // Queries are valid before boot and while the user visits the library.
    unsigned previous = nBurnDrvActive;
    nBurnDrvActive = index;
    BurnRomInfo rom = {};
    char *label = nullptr;
    int err = BurnDrvGetRomInfo(&rom, number) || BurnDrvGetRomName(&label, number, 0);
    nBurnDrvActive = previous;
    if (err) return 1;
    if (name) *name = label;
    bool required = !(rom.nType & (BRF_OPT | BRF_NODUMP));
    if (size) *size = required ? rom.nLen : 0;
    if (crc) *crc = required ? rom.nCrc : 0;
    return 0;
}

extern "C" void astra_musashi_dynasty(unsigned char *);

static int openGame(int index, int (*load)(unsigned char *, int *, int), int rate, int capacity)
{
    BurnLibInit();
    nBurnDrvActive = index;
    BurnExtLoadRom = load;
    nBurnBpp = 2;
    nBurnPitch = 384 * 2;
    nBurnSoundLen = capacity;
    nBurnSoundRate = rate;
    nCurrentFrame = 0;
    int result = BurnDrvInit();
    BurnExtLoadRom = nullptr;
    if (!result) {
        const uint16_t task[] = {0x422d,0x0104,0x41ed,4,0x303c,15,0x46fc,0x2600,
            0x4a2d,0x0104,0x66ea,0x1228,0,0x0c01,4,0x6418,0x46fc,0x2000,
            0x41e8,16,0x51c8,0xffe2};
        // The local dynwaru archive is named dynwara by this driver.
        bool dynasty = !strcmp(BurnDrvGetTextA(DRV_NAME), "dynwara") &&
            nCpsRomLen >= 0x64e && !memcmp(CpsRom + 0x622, task, sizeof(task));
        astra_musashi_dynasty(dynasty ? CpsRamFF : nullptr);
        BurnRecalcPal();
    }
    return result;
}
static void closeGame()
{
    BurnDrvExit();
    BurnLibExit();
    pBurnDraw = nullptr;
    pBurnSoundOut = nullptr;
}
static int frame(uint16_t *pixels, int16_t *samples, int count)
{
    pBurnDraw = reinterpret_cast<unsigned char *>(pixels);
    pBurnSoundOut = samples;
    nSkipFrame = !pixels;
    nBurnSoundLen = count;
    int result = BurnDrvFrame();
    ++nCurrentFrame;
    return result;
}
static void reset(bool on) { CpsReset = on; }

extern "C" const AstraExtraApi astra_extra_api = {
    vertical, chip, openGame, closeGame, frame, reset, BurnDrvGetInputInfo, BurnDrvGetDIPInfo
};
