// RGB565 tile cache for the CPS1 draw path. The immutable ROM supplies pens;
// a palette revision and the priority mask determine their visible colours.
#include <stdint.h>

namespace {
template<int Width> struct TileImage {
    unsigned identity;
    unsigned paletteRevision;
    unsigned mask;
    unsigned rawRows;
    unsigned occupiedRows;
    uint32_t rowPixels[Width];
    uint16_t pixels[Width][Width];
};

// Separate sizes keep the small text and object tiles from evicting large
// background tiles. Each set has four ways for neighbouring tilemap entries.
template<int Width, int Sets> struct TileCache {
    TileImage<Width> images[Sets][4];
    uint8_t nextWay[Sets];
};
struct TileStore {
    TileCache<8, 32> textTiles;
    TileCache<16, 128> objectTiles;
    TileCache<32, 64> backgroundTiles;
};
TileStore *tiles;
unsigned paletteVersions[128];

inline uint32_t lowBits(int count)
{
    return count == 32 ? UINT32_MAX : (1u << count) - 1;
}

template<int Width>
void decodeTile(TileImage<Width> &tile, const uint8_t *source, bool flip, unsigned mask)
{
    const int stride = Width == 32 ? 16 : 8;
    tile.rawRows = tile.occupiedRows = 0;
    for (int y = 0; y < Width; ++y, source += stride) {
        const uint32_t *words = reinterpret_cast<const uint32_t *>(source);
        unsigned raw = 0;
        uint32_t visible = 0;
        for (int group = 0; group < Width / 8; ++group) {
            uint32_t pens = words[group];
            raw |= pens;
            if (!pens) continue;
            for (int i = 0; i < 8; ++i, pens <<= 4) {
                const unsigned pen = pens >> 28;
                if (!(mask & (1u << pen))) continue;
                const int pos = group * 8 + i;
                const int x = flip ? Width - pos - 1 : pos;
                visible |= 1u << x;
                tile.pixels[y][x] = static_cast<uint16_t>(CpstPal[pen]);
            }
        }
        tile.rowPixels[y] = visible;
        if (raw) tile.rawRows |= 1u << y;
        if (visible) tile.occupiedRows |= 1u << y;
    }
}

template<int Width, int Sets>
TileImage<Width> &lookupTile(TileCache<Width, Sets> &cache, unsigned addr, unsigned mask)
{
    constexpr unsigned tileBytes = Width == 8 ? 64 : Width * Width / 2;
    const unsigned number = addr / tileBytes;
    const unsigned bank = static_cast<unsigned>(CpstPal - CpsPal) >> 4;
    const unsigned flip = nCpstFlip & 1;
    const unsigned identity = (addr << 8) | (bank << 1) | flip;
    const unsigned index = (number ^ (number >> 6) ^ (bank * 17)) & (Sets - 1);
    TileImage<Width> *set = cache.images[index];
    for (int i = 0; i < 4; ++i) {
        if (set[i].identity == identity && set[i].mask == mask &&
            set[i].paletteRevision == paletteVersions[bank]) return set[i];
    }
    TileImage<Width> &tile = set[cache.nextWay[index]++ & 3];
    tile.identity = identity;
    tile.mask = mask;
    tile.paletteRevision = paletteVersions[bank];
    decodeTile(tile, CpsGfx + addr, flip, mask);
    return tile;
}

template<int Width>
inline void plotRow(uint16_t *dest, const uint16_t *src, uint32_t visible)
{
    if (visible == lowBits(Width)) {
        __builtin_memcpy(dest, src, Width * 2);
        return;
    }
    // Copy runs of opaque pixels. All transparent rows were already rejected.
    while (visible) {
        unsigned first = __builtin_ctz(visible);
        uint32_t atFirst = visible >> first;
        unsigned count = __builtin_ctz(~atFirst);
        if (count == 1) dest[first] = src[first];
        else memcpy(dest + first, src + first, count * sizeof(uint16_t));
        visible &= ~(lowBits(count) << first);
    }
}

template<int Width, int Sets>
int drawTile(TileCache<Width, Sets> &cache, unsigned mask)
{
    int top = nCpstY < 0 ? -nCpstY : 0;
    int end = nCpstY + Width > 224 ? 224 - nCpstY : Width;
    const bool rows = (nCpstType & CTT_ROWS) != 0;
    if (top >= end) return rows ? 1 : 0;
    if (!rows && (nCpstX <= -Width || nCpstX >= 384)) return 0;
    const unsigned address = nCpstTile & nCpsGfxMask;
    if (address >= nCpsGfxLen) return 1;
    // For a zero-priority pass, read the normal tile's blank-line metadata.
    // The caller remembers blank tiles, even when priority hides every pen.
    TileImage<Width> &tile = lookupTile(cache, address, mask ? mask : 0xfffeu);
    const bool flipY = (nCpstFlip & 2) != 0;
    const uint32_t range = lowBits(end - top) << (flipY ? Width - end : top);
    if (!(tile.rawRows & range)) return 1;
    if (!mask || !(tile.occupiedRows & range)) return 0;
    for (int y = top; y < end; ++y) {
        int sy = flipY ? Width - 1 - y : y;
        uint32_t visible = tile.rowPixels[sy];
        if (!visible) continue;
        int left = nCpstX + (rows ? CpstRowShift[y] : 0);
        if (left <= -Width || left >= 384) continue;
        if (left < 0) visible &= ~lowBits(-left);
        if (left + Width > 384) visible &= lowBits(384 - left);
        auto *dest = reinterpret_cast<uint16_t *>(pBurnDraw + (nCpstY + y) * nBurnPitch) + left;
        plotRow<Width>(dest, tile.pixels[sy], visible);
    }
    return 0;
}

int renderTile(unsigned mask)
{
    switch (nCpstType & 24) {
        case CTT_8X8: return drawTile(tiles->textTiles, mask);
        case CTT_16X16: return drawTile(tiles->objectTiles, mask);
        default: return drawTile(tiles->backgroundTiles, mask);
    }
}
int renderNormalTile() { return tiles ? renderTile(0xfffeu) : CpstOne(); }
int renderPriorityTile()
{
    // The legacy high-priority table has no row-scroll entries. Its no-op
    // return also participates in the caller's last-blank-tile bookkeeping.
    if (!tiles || (nCpstType & CTT_ROWS)) return CpstOneBgHi();
    // CPS priority bits index hardware pens; PiFBA stores (hardware pen XOR 15).
    unsigned mask = CpstPmsk & 0xffff;
    mask = ((mask & 0x5555) << 1) | ((mask >> 1) & 0x5555);
    mask = ((mask & 0x3333) << 2) | ((mask >> 2) & 0x3333);
    mask = ((mask & 0x0f0f) << 4) | ((mask >> 4) & 0x0f0f);
    return renderTile(((mask << 8) | (mask >> 8)) & 0xfffe);
}
}

void AstraTileOpen()
{
    free(tiles);
    tiles = static_cast<TileStore *>(calloc(1, sizeof(TileStore)));
}
void AstraTileClose() { free(tiles); tiles = NULL; }
const unsigned *AstraTileBankVersions() { return paletteVersions; }
void AstraTilePaletteChanged(unsigned bank) { ++paletteVersions[bank]; }
void AstraTilePaletteReset()
{
    for (unsigned &revision : paletteVersions) ++revision;
}
