#include "settings.h"
#include "ui_language.h"
#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include "menu.h"
#include "display.h"
#include "cps1.h"

enum { PAGE_SIZE = 7 };

static void paint_menu(const RomLibrary *library, int selected)
{
    printf(ui_text("\x1b[2J\x1b[1;1H\x1b[1;36mASTRA CPS1\x1b[0m   /   JEUX SUR LA SD\n\n", "\x1b[2J\x1b[1;1H\x1b[1;36mASTRA CPS1\x1b[0m   /   SD CARD GAMES\n\n"));
    if (!library->count) {
        printf(ui_text("Place tes archives .zip dans\n/3ds/AstraCPS1/roms/\n\n", "Place your .zip archives in\n/3ds/AstraCPS1/roms/\n\n"));
    } else {
        int first = selected / PAGE_SIZE * PAGE_SIZE;
        for (int i = first; i < first + PAGE_SIZE && (size_t)i < library->count; ++i) {
            const RomEntry *rom = &library->entries[i];
            const char *title = rom->game < 0 ? library_filename(rom->path) : cps1_game_title(rom->game);
            printf("%s%c %.36s\x1b[0m\n", i == selected ? "\x1b[1;36m" : "",
                   i == selected ? '>' : ' ', title);
        }
        const RomEntry *rom = &library->entries[selected];
        printf("\x1b[12;1H%u / %u\n", (unsigned)(selected + 1), (unsigned)library->count);
        if (rom->game >= 0) {
            const char *title = cps1_game_title(rom->game);
            for (size_t off = 0; off < strlen(title); off += 38)
                printf("%.38s\n", title + off);
        } else {
            printf(ui_text("Identification au lancement\n", "Identified when launched\n"));
        }
        printf("\n%.39s\n", library_filename(rom->path));
        if (rom->game >= 0 && cps1_game_parent(rom->game))
            printf(ui_text("Parent si set separe : %.15s.zip\n", "Split set parent: %.15s.zip\n"), cps1_game_parent(rom->game));
    }
    if (library->incomplete) printf(ui_text("\nMemoire insuffisante : liste partielle\n", "\nLow memory: partial game list\n"));
    printf("\x1b[21;1H\x1b[1;36mY   Francais / English\x1b[0m");
    printf(ui_text("\x1b[23;1HHaut / bas     Choisir\nGauche/droite  Changer de page\n", "\x1b[23;1HUp / down      Select\nLeft / right   Change page\n"));
    printf(ui_text("A              Lancer\nX              Relire la carte\nB              Quitter\n", "A              Launch\nX              Rescan SD card\nB              Exit\n"));
    printf("\x1b[29;1H\x1b[1;36m%-14s%-13s%s\x1b[0m", "  < PAGE", ui_text("  LANCER", "  LAUNCH"), "  PAGE >");
    display_flush_console();
}

int menu_choose(RomLibrary *library, int selected)
{
    if (selected < 0 || (size_t)selected >= library->count) selected = 0;
    hidSetRepeatParameters(20, 5);
    bool dirty = true;
    while (aptMainLoop()) {
        if (dirty) paint_menu(library, selected);
        dirty = false;
        hidScanInput();
        u32 down = hidKeysDown(), move = down | hidKeysDownRepeat();
        if (down & KEY_B) break;
        if ((down & KEY_A) && library->count) return selected;
        if (down & KEY_Y) { settings_language(); dirty = true; continue; }
        if (down & KEY_TOUCH) {
            touchPosition tap;
            hidTouchRead(&tap);
            if (tap.py >= 156 && tap.py < 172) { settings_language(); dirty = true; continue; }
            if (!library->count) continue;
            if (tap.py >= 216) {
                if (tap.px < 106) move |= KEY_LEFT;
                else if (tap.px >= 213) move |= KEY_RIGHT;
                else return selected;
            } else if (tap.py >= 16 && tap.py < (2 + PAGE_SIZE) * 8) {
                int index = selected / PAGE_SIZE * PAGE_SIZE + tap.py / 8 - 2;
                if ((size_t)index < library->count) {
                    selected = index;
                    dirty = true;
                }
            }
        }
        if (down & KEY_X) {
            library_scan(library);
            selected = 0;
            dirty = true;
        } else if (library->count) {
            int next = selected, count = (int)library->count;
            if (move & KEY_UP) next = (selected + count - 1) % count;
            else if (move & KEY_DOWN) next = (selected + 1) % count;
            else if (move & KEY_LEFT) next = selected > PAGE_SIZE ? selected - PAGE_SIZE : 0;
            else if (move & KEY_RIGHT) next = selected + PAGE_SIZE < count ? selected + PAGE_SIZE : count - 1;
            dirty |= next != selected;
            selected = next;
        }
        display_service();
        gspWaitForVBlank();
    }
    return -1;
}

bool menu_confirm_missing_samples(void)
{
    printf(ui_text("\x1b[2J\x1b[1;1HWO FH   /   SON INCOMPLET\n\n", "\x1b[2J\x1b[1;1HWO FH   /   MISSING AUDIO SAMPLES\n\n"));
    printf(ui_text("La puce des voix et bruitages manque.\n"
           "Pour le son complet, placer sgyxz.zip\n"
           "avec sgyxz_snd1.bin (CRC c15ac0f2)\n"
           "dans le meme dossier.\n\n"
           "A   Essayer sans ces sons\nB   Retour aux jeux", "Voice and effects chip missing.\nFor full audio, place sgyxz.zip with\nsgyxz_snd1.bin (CRC c15ac0f2)\nin the same directory.\n\nA   Try without these sounds\nB   Back to games"));
    display_flush_console();
    while (aptMainLoop()) {
        hidScanInput();
        u32 down = hidKeysDown();
        if (down & KEY_B) return false;
        if (down & KEY_A) return true;
        gspWaitForVBlank();
    }
    return false;
}

void menu_error(const char *path, const char *reason)
{
    printf(ui_text("\x1b[2J\x1b[1;1H\x1b[1;36mASTRA CPS1\x1b[0m   /   CHARGEMENT\n\n", "\x1b[2J\x1b[1;1H\x1b[1;36mASTRA CPS1\x1b[0m   /   LOADING\n\n"));
    printf(ui_text("%.39s\n\n%s\n\nA / B   Retour aux jeux\n", "%.39s\n\n%s\n\nA / B   Back to games\n"), library_filename(path), reason);
    display_flush_console();
    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & (KEY_A | KEY_B)) return;
        display_service();
        gspWaitForVBlank();
    }
}
