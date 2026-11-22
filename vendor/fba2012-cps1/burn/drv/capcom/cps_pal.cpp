#include "cps.h"

/* CPS (palette) */

UINT32* CpsPal = NULL;					/* Hicolor version of palette */
INT32 nCpsPalCtrlReg;
INT32 bCpsUpdatePalEveryFrame = 0;	/* Some of the hacks need this as they don't write to CpsReg 0x0a */

#define BIT(x,n) 		(((x)>>(n))&1)

void AstraTileOpen();
void AstraTileClose();
void AstraTilePaletteChanged(unsigned);
static UINT16 previous[6][0x200];
static bool validPage[6];

INT32 CpsPalInit()
{
   INT32 nLen = 0xc00 * sizeof(UINT32);
   CpsPal = (UINT32*)BurnMalloc(nLen);
   if (!CpsPal)
      return 1;

   memset(validPage, 0, sizeof(validPage));
   AstraTileOpen();
   return 0;
}

INT32 CpsPalExit()
{
	AstraTileClose();
	BurnFree(CpsPal);
	return 0;
}

/* Update CpsPal with the new palette at pNewPal (length 0xc00 bytes) */
INT32 CpsPalUpdate(UINT8* pNewPal)
{
   INT32 nPage;
   INT32 nCtrl = CpsReg[nCpsPalCtrlReg];
   UINT16 *PaletteRAM = (UINT16*)pNewPal;

   for (nPage = 0; nPage < 6; nPage++)
   {
      if (BIT(nCtrl, nPage))
      {
         INT32 Offset;
         for (INT32 bank = 0; bank < 32; ++bank) {
            UINT16 *old = previous[nPage] + bank * 16;
            if (validPage[nPage] && !memcmp(old, PaletteRAM, 32)) {
               PaletteRAM += 16;
               continue;
            }
            memcpy(old, PaletteRAM, 32);
            if (nPage < 4) AstraTilePaletteChanged(nPage*32 + bank);
            for (Offset = bank * 16; Offset < (bank+1)*16; ++Offset) {
            INT32 Palette = BURN_ENDIAN_SWAP_INT16(*(PaletteRAM++));
            INT32 Bright = 0x0f + ((Palette >> 12) << 1);
            INT32      r = ((Palette >> 8) & 0x0f) * 0x11 * Bright / 0x2d;
            INT32      g = ((Palette >> 4) & 0x0f) * 0x11 * Bright / 0x2d;
            INT32      b = ((Palette >> 0) & 0x0f) * 0x11 * Bright / 0x2d;

            CpsPal[(0x200 * nPage) + (Offset ^ 15)] = BurnHighCol(r, g, b, 0);
            }
         }
         validPage[nPage] = true;
      }
   }

   return 0;
}
