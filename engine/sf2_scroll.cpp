#include "cps.h"
#include <algorithm>
#include <stdint.h>

// Cache decoded strips in the coordinate system of each CPS tilemap. A bit
// per pixel distinguishes a transparent pen from an opaque shade of black.
const unsigned *AstraTileBankVersions();
namespace {
struct TileTag {
    unsigned mapWord, paletteVersion;
    bool valid;
};
template<unsigned Width> struct Strip {
    uint16_t pixels[Width][64 * Width];
    uint32_t coverage[Width][2 * Width];
    TileTag tags[64];
};
struct Planes {
    Strip<8> text[32];
    Strip<16> middle[16];
    Strip<32> back[8];
};
Planes *planes;

// The generic ARMv6 memcpy falls back to bytes when source and destination
// have opposite halfword alignment, a common case while the camera moves.
void copyLine(uint16_t *dst, const uint16_t *src, unsigned pixels)
{
    if (pixels && ((uintptr_t)dst & 2)) { *dst++ = *src++; --pixels; }
    if (((uintptr_t)src & 2) && pixels >= 9) {
        // Keep one source pixel in hand. Only load whole aligned words inside
        // the span, including at the rightmost edge of the last cached row.
        uint32_t carry = *src++;
        auto *out = reinterpret_cast<uint32_t *>(dst);
        const auto *in = reinterpret_cast<const uint32_t *>(src);
        while (pixels >= 9) {
            for (int i = 0; i < 4; ++i) {
                const uint32_t word = *in++;
                *out++ = carry | (word << 16);
                carry = word >> 16;
            }
            pixels -= 8;
        }
        dst = reinterpret_cast<uint16_t *>(out);
        src = reinterpret_cast<const uint16_t *>(in);
        *dst++ = carry;
        --pixels;
    }
#ifdef __arm__
    else if (pixels >= 16 && !((uintptr_t)src & 3)) {
        unsigned blocks = pixels / 16;
        asm volatile("1: ldmia %1!, {r3-r9, r12}\n"
                     "stmia %0!, {r3-r9, r12}\n"
                     "subs %2, %2, #1\n" "bne 1b"
                     : "+&r"(dst), "+&r"(src), "+&r"(blocks)
                     : : "r3","r4","r5","r6","r7","r8","r9","r12","cc","memory");
        pixels %= 16;
    }
#endif
    while (pixels--) *dst++ = *src++;
}

template<unsigned W>
void refreshTile(Strip<W> &strip, unsigned x, unsigned y, const unsigned char *base,
                 const unsigned *versions)
{
    constexpr unsigned plane = W == 8 ? 1 : W == 16 ? 2 : 3;
    constexpr unsigned tileBytes = W == 8 ? 64 : W*W/2;
    constexpr unsigned groupRows = 256/W;
    unsigned offset = ((y & (64-groupRows)) << 8) | (x * groupRows * 4) | ((y % groupRows)*4);
    const auto *entry = reinterpret_cast<const uint16_t *>(base + offset);
    unsigned code = entry[0];
    const unsigned attr = entry[1], bank = 32*plane + (attr & 31);
    const unsigned revision = versions[bank], descriptor = code | (attr << 16);
    TileTag &tag = strip.tags[x];
    if (tag.valid && tag.mapWord == descriptor && tag.paletteVersion == revision) return;
    tag = {descriptor, revision, true};

    if (W == 8 && Knights && code == 0xf020) code = 0x8820;
    const unsigned address = (code*tileBytes + nCpsGfxScroll[plane]) & nCpsGfxMask;
    bool blank = W == 8 ? (code == 0x20 || (Msword && code == 0xf020)) :
        (int)code < StartScroll[plane-2] || (int)code > EndScroll[plane-2];
    blank |= address + tileBytes > (unsigned)nCpsGfxLen;
    const unsigned *palette = CpsPal + bank*16;
    const unsigned bit = x*W % 32;
    constexpr uint32_t tileMask = UINT32_MAX >> (32-W);
    for (unsigned row = 0; row < W; ++row) {
        const unsigned destRow = attr & 64 ? W-1-row : row;
        uint16_t *dest = strip.pixels[destRow] + x*W;
        uint32_t mask = 0;
        if (blank) {
            memset(dest, 0, W*sizeof(uint16_t));
        } else {
            const auto *words = reinterpret_cast<const uint32_t *>(
                CpsGfx + address + row*(W == 8 ? 8 : W/2));
            for (unsigned group = 0; group < W/8; ++group) {
                uint32_t bits = words[group];
                for (unsigned i = 0; i < 8; ++i, bits <<= 4) {
                    unsigned pen = bits >> 28, pos = group*8+i;
                    unsigned at = attr & 32 ? W-1-pos : pos;
                    dest[at] = pen ? palette[pen] : 0;
                    if (pen) mask |= 1u << at;
                }
            }
        }
        uint32_t &word = strip.coverage[destRow][x*W/32];
        word = (word & ~(tileMask << bit)) | (mask << bit);
    }
}

// Consecutive fully covered words usually describe the floor or the sky.
// Copy them together. On a partially transparent edge only copy visible pens.
void overlayLine(uint16_t *dst, const uint16_t *row, const uint32_t *masks,
                 unsigned start, unsigned length)
{
    unsigned end = start + length;
    while (start < end) {
        unsigned bit = start & 31;
        unsigned n = std::min(32-bit, end-start);
        unsigned wanted = UINT32_MAX >> (32-n);
        unsigned visible = (masks[start/32] >> bit) & wanted;
        if (visible == wanted) {
            while (end-start-n >= 32 && masks[(start+n)/32] == UINT32_MAX) n += 32;
            copyLine(dst, row+start, n);
        } else if (visible) {
            while (visible) {
                unsigned skip = __builtin_ctz(visible);
                unsigned run = __builtin_ctz(~(visible >> skip));
                if (run == 1) dst[skip] = row[start+skip];
                else copyLine(dst+skip, row+start+skip, run);
                visible &= ~((UINT32_MAX >> (32-run)) << skip);
            }
        } else {
            while (end-start-n >= 32 && !masks[(start+n)/32]) n += 32;
        }
        dst += n; start += n;
    }
}

template<unsigned W, unsigned Bands>
void paint(Strip<W> (&cache)[Bands], const unsigned char *base, int sx, int sy, bool opaque)
{
    const unsigned *versions = AstraTileBankVersions();
    const int firstY = -(sy & (W-1));
    for (int band = 0, y = firstY; y < 224; ++band, y += W) {
        int mapY = (sy / int(W)) + band;
        Strip<W> &strip = cache[mapY % Bands];
        // Scroll 2 alone has per-line horizontal shifts.
        const struct CpsrLineInfo *line = W == 16 ? &CpsrLineInfo[band] : nullptr;
        int tileX = line ? line->nTileStart : sx/int(W);
        int count = line && line->nWidth ? line->nTileEnd - tileX : 384/int(W)+1;
        for (int i = 0; i < count && i < 64; ++i)
            refreshTile(strip, (tileX+i)&63, mapY, base, versions);
        for (int r = std::max(0,-y); r < std::min(int(W),224-y); ++r) {
            int shift = line ? (line->nWidth ? line->Rows[r] : -(line->nStart & 15)) : -(sx & int(W-1));
            int left = std::min(384,std::max(0,shift));
            int right = std::max(left,std::min(384,count*int(W)+shift));
            int src = (tileX*int(W)-shift+left) & (64*W-1);
            int run = std::min(right-left,int(64*W)-src);
            auto *dst = reinterpret_cast<uint16_t *>(pBurnDraw+(y+r)*nBurnPitch);
            if (opaque) {
                if (left) memset(dst,0,left*2);
                copyLine(dst+left,strip.pixels[r]+src,run);
                if (right-left>run) copyLine(dst+left+run,strip.pixels[r],right-left-run);
                if (right<384) memset(dst+right,0,(384-right)*2);
            } else {
                overlayLine(dst+left,strip.pixels[r],strip.coverage[r],src,run);
                if (right-left>run) overlayLine(dst+left+run,strip.pixels[r],strip.coverage[r],0,right-left-run);
            }
        }
    }
}
}

void AstraSf2StripsOpen(bool enable)
{
    free(planes);
    planes = enable ? static_cast<Planes *>(calloc(1, sizeof(Planes))) : nullptr;
}

bool AstraPaintSf2Plane(int layer, bool opaque)
{
    if (!planes || nBurnPitch != 768 || nBurnBpp != 2) return false;
    if (layer == 2 && CpsrBase) {
        paint(planes->middle,CpsrBase,nCpsrScrX,nCpsrScrY,opaque);
        return true;
    }
    if (layer != 1 && layer != 3) return false;
    const auto *regs = reinterpret_cast<const uint16_t *>(CpsSaveReg[0]);
    const unsigned char *base = CpsFindGfxRam((regs[layer] << 8) & 0xffc000,0x4000);
    if (!base) return false;
    int sx = regs[4+layer*2] + 0x40, sy = regs[5+layer*2] + 0x10;
    if (layer == 1) paint(planes->text,base,sx,sy,opaque);
    else paint(planes->back,base,sx,sy,opaque);
    return true;
}

bool AstraSf2HasPriorityPens()
{
    if (!planes) return true;
    const unsigned char *regs = CpsSaveReg[0];
    for (int group = 0; group < 4; ++group)
        if (*reinterpret_cast<const uint16_t *>(regs + MaskAddr[group]) & 0x7fff) return true;
    return false;
}
