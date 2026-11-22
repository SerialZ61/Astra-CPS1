// Preserve the opaque composite below the sprites when the relevant tilemaps,
// palette banks, row shifts and scroll/control registers are byte-identical.
// Upper layers and sprites continue through the normal priority ordering.
const unsigned *AstraTileBankVersions();
namespace {
struct LowerKey {
    unsigned order;
    unsigned layers;
    unsigned scroll[3];
    unsigned maps[3];
    unsigned rowBase;
    unsigned rowStart;
    unsigned banks[3][32];
};
struct LowerPicture {
    LowerKey key;
    unsigned char maps[3][0x4000];
    unsigned char rows[0x800];
    unsigned short picture[224][384];
    bool haveMaps;
    bool havePicture;
};
LowerPicture *lower;
enum LowerAction { drawLower, recordLower, reuseLower };

// All tilemap and image transfers here are multiples of 32 bytes. ARM11 can
// move eight aligned words in one load/store pair.
__attribute__((noinline)) void copyBlocks(void *dest, const void *src, unsigned bytes)
{
#ifdef __arm__
    if (!((reinterpret_cast<uintptr_t>(dest) | reinterpret_cast<uintptr_t>(src)) & 3u)) {
        asm volatile("1: ldmia %1!, {r3-r9, r12}\n"
                     "stmia %0!, {r3-r9, r12}\n"
                     "subs %2, %2, #32\n"
                     "bne 1b"
                     : "+&r"(dest), "+&r"(src), "+&r"(bytes)
                     : : "r3","r4","r5","r6","r7","r8","r9","r12","cc","memory");
        return;
    }
#endif
    memcpy(dest, src, bytes);
}

bool equalMap(const void *left, const void *right, unsigned bytes)
{
    // Map images and saved row tables are word aligned. Compare sixteen bytes
    // per iteration, with an early exit on the first change.
    const auto *a = static_cast<const unsigned *>(left);
    const auto *b = static_cast<const unsigned *>(right);
    for (unsigned n = bytes / 16; n; --n, a += 4, b += 4) {
        if ((a[0] ^ b[0]) | (a[1] ^ b[1]) | (a[2] ^ b[2]) | (a[3] ^ b[3])) return false;
    }
    return true;
}

unsigned scrollReg(int offset)
{
    return *reinterpret_cast<const unsigned short *>(CpsSaveReg[0] + offset);
}

LowerAction inspectLower(unsigned layers, unsigned order)
{
    if (!lower || !layers || CpsStar || (order & (CpsLayEn[4] | CpsLayEn[5])) ||
        nBurnPitch != 384 * 2 || nBurnBpp != 2) return drawLower;
    LowerKey key = {};
    key.layers = layers;
    key.order = order;
    const unsigned char *maps[3] = {};
    for (unsigned n = 0; n < 3; ++n) {
        if (!(layers & (2u << n))) continue;
        key.scroll[n] = scrollReg(0x0c + n * 4) | (scrollReg(0x0e + n * 4) << 16);
        key.maps[n] = (scrollReg(2 + n * 2) << 8) & 0xffc000;
        maps[n] = CpsFindGfxRam(key.maps[n], 0x4000);
        memcpy(key.banks[n], AstraTileBankVersions() + (n + 1) * 32, sizeof(key.banks[n]));
    }
    const unsigned short *rowData = NULL;
    if ((layers & 4) && CpsrBase && CpsrRows) {
        rowData = CpsrRows;
        key.rowBase = (scrollReg(0x08) << 8) & 0xfff800;
        key.rowStart = nCpsrRowStart;
    }
    if (memcmp(&key, &lower->key, sizeof(key))) {
        lower->key = key;
        lower->haveMaps = lower->havePicture = false;
        return drawLower;
    }

    bool unchanged = lower->haveMaps;
    for (unsigned n = 0; n < 3; ++n) {
        if (maps[n] && (!lower->haveMaps || !equalMap(lower->maps[n], maps[n], 0x4000))) {
            copyBlocks(lower->maps[n], maps[n], 0x4000);
            unchanged = false;
        }
    }
    if (rowData && (!lower->haveMaps || !equalMap(lower->rows, rowData, sizeof(lower->rows)))) {
        copyBlocks(lower->rows, rowData, sizeof(lower->rows));
        unchanged = false;
    }
    lower->haveMaps = true;
    if (!unchanged) {
        lower->havePicture = false;
        return drawLower;
    }
    return lower->havePicture ? reuseLower : recordLower;
}
}
void AstraLowerOpen()
{
    free(lower);
    lower = static_cast<LowerPicture *>(calloc(1, sizeof(LowerPicture)));
}
void AstraLowerClose() { free(lower); lower = NULL; }
