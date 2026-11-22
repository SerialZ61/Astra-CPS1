#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include "settings.h"
#include "cps1.h"
#include "display.h"

enum { ROWS = 8, MAX_DIP = 64, LABEL = 40, SAVE = -2 };
typedef char Text[LABEL];

static const char *translation(const char *s)
{
    static const struct { const char *from, *to; } words[] = {
        {"Difficulty", "Difficulte"}, {"Game Level", "Difficulte"},
        {"Difficulty 2", "Difficulte 2"}, {"Free Play", "Jeu gratuit"},
        {"Lives", "Vies"}, {"Bonus Life", "Vie bonus"}, {"Freeze", "Gel du jeu"},
        {"Flip", "Inversion ecran"}, {"Demo Sound", "Son de demo"},
        {"Continue", "Continuer"}, {"A Side", "Monnayeur A"}, {"B Side", "Monnayeur B"},
        {"Continue Coin", "Tarif pour continuer"}, {"On", "Active"}, {"Off", "Desactive"}
    };
    if (!s) return "-";
    for (size_t i = 0; i < sizeof(words)/sizeof(*words); ++i)
        if (!strcmp(words[i].from, s)) return words[i].to;
    return s;
}

// Each entry owns two text rows (16 touch pixels). Primary actions can also
// be completed with the stylus alone, even if every physical key was remapped.
static int pick(const char *title, const char *detail, const Text *labels, int count,
                int selected, bool save, const char *hint)
{
    if (selected < 0 || selected >= count) selected = 0;
    bool dirty = true;
    while (aptMainLoop()) {
        int first = selected / ROWS * ROWS;
        if (dirty) {
            printf("\x1b[2J\x1b[1;1H\x1b[1;36m%.38s\x1b[0m\n%.39s\n", title, detail);
            for (int i = first; i < first + ROWS && i < count; ++i) {
                printf("\x1b[%d;1H%s%c %.36s\x1b[0m", 5 + 2*(i-first),
                       i == selected ? "\x1b[1;36m" : "", i == selected ? '>' : ' ', labels[i]);
            }
            printf("\x1b[22;1H%.39s\n", hint);
            printf("A choisir   B retour   Page %d/%d\n", first/ROWS+1, (count+ROWS-1)/ROWS);
            if (save) printf("START enregistrer\n");
            printf("\x1b[28;1H\x1b[1;36m%-10s%-10s%-10s%s\x1b[0m",
                   "  < PAGE", " RETOUR", save ? " SAUVER" : "", "  PAGE >");
            display_flush_console();
            dirty = false;
        }
        hidScanInput();
        u32 down = hidKeysDown(), move = down | hidKeysDownRepeat();
        if (down & KEY_B) return -1;
        if (save && (down & KEY_START)) return SAVE;
        if (down & KEY_A) return selected;
        if (down & KEY_TOUCH) {
            touchPosition tap;
            hidTouchRead(&tap);
            if (tap.py >= 216) {
                if (tap.px < 80) move |= KEY_LEFT;
                else if (tap.px < 160) return -1;
                else if (tap.px < 240) { if (save) return SAVE; }
                else move |= KEY_RIGHT;
            } else if (tap.py >= 32 && tap.py < 32 + ROWS * 16) {
                int touched = first + (tap.py - 32) / 16;
                if (touched < count) return touched;
            }
        }
        int next = selected;
        if (move & KEY_UP) next = (selected + count - 1) % count;
        else if (move & KEY_DOWN) next = (selected + 1) % count;
        else if (move & KEY_LEFT) next = selected > ROWS ? selected - ROWS : 0;
        else if (move & KEY_RIGHT) next = selected + ROWS < count ? selected + ROWS : count-1;
        dirty = next != selected;
        selected = next;
        display_service();
        gspWaitForVBlank();
    }
    return -1;
}

static bool save_problem(void)
{
    const Text options[] = {"Reessayer", "Annuler"};
    return pick("SAUVEGARDE IMPOSSIBLE", "Verifie l'ecriture de la carte SD.", options,
                2, 0, false, "La partie reste en pause.") == 0;
}

static void edit_buttons(int game, Bindings *active)
{
    const char *id = cps1_game_id(game);
    const int actions = cps1_game_buttons(game);
    const bool ffight = !strncmp(id, "ffight", 6);
    Bindings draft = *active;
    int row = 0;
    for (;;) {
        Text keys[BIND_SLOT_COUNT + 1];
        for (int i = 0; i < BIND_SLOT_COUNT; ++i)
            snprintf(keys[i], LABEL, "%-11s %.24s", bindings_key_name(i),
                     bindings_action_name(draft.action[i], actions, ffight));
        strcpy(keys[BIND_SLOT_COUNT], "Retablir les touches par defaut");
        row = pick("TOUCHES", cps1_game_title(game), keys, BIND_SLOT_COUNT + 1,
                   row, true, "Choisis un bouton a reaffecter.");
        if (row == SAVE) {
            if (memcmp(&draft, active, sizeof(draft)) == 0) return;
            while (!bindings_save(&draft, id)) if (!save_problem()) return;
            *active = draft;
            return;
        }
        if (row < 0) return;
        if (row == BIND_SLOT_COUNT) {
            bindings_defaults(&draft, actions);
            continue;
        }
        Text choices[BIND_ACTION_COUNT];
        BindAction map[BIND_ACTION_COUNT];
        int n = 0, selected = 0;
        for (int a = 0; a < BIND_ACTION_COUNT; ++a) {
            if (!bindings_allowed((BindAction)a, actions)) continue;
            if (a == draft.action[row]) selected = n;
            map[n] = (BindAction)a;
            snprintf(choices[n++], LABEL, "%s", bindings_action_name((BindAction)a, actions, ffight));
        }
        int value = pick("AFFECTER UNE ACTION", bindings_key_name(row), choices, n,
                         selected, false, "Choisis l'action de ce bouton.");
        if (value >= 0) draft.action[row] = map[value];
    }
}

// Fingerprint the driver's own DIP schema so a later change in group order or
// option labels cannot reinterpret a previous version's saved settings.
typedef struct {
    uint32_t schema;
    uint8_t count;
    uint8_t option[MAX_DIP];
    uint8_t reserved[3];
} DipProfile;

static uint32_t hash_name(uint32_t hash, const char *name)
{
    if (name) for (const unsigned char *s = (const unsigned char *)name; *s; ++s)
        hash = (hash ^ *s) * 16777619u;
    return (hash ^ 255u) * 16777619u;
}

static DipProfile current_dips(void)
{
    DipProfile profile = { .schema = 2166136261u };
    int count = cps1_dip_count();
    if (count > MAX_DIP) return profile;
    profile.count = count;
    for (int i = 0; i < count; ++i) {
        profile.schema = hash_name(profile.schema, cps1_dip_name(i));
        for (int j = 0; j < cps1_dip_option_count(i); ++j)
            profile.schema = hash_name(profile.schema, cps1_dip_option_name(i,j));
        profile.option[i] = (uint8_t)cps1_dip_get(i);
    }
    return profile;
}

static void set_dips(const DipProfile *p)
{
    for (int i = 0; i < p->count; ++i)
        if (p->option[i] != 255) cps1_dip_set(i, p->option[i]);
}

void settings_load_dips(int game)
{
    DipProfile saved, current = current_dips();
    if (!profile_read(cps1_game_id(game), "dip", &saved, sizeof(saved)) ||
        saved.schema != current.schema || saved.count != current.count) return;
    for (int i = 0; i < saved.count; ++i)
        if (saved.option[i] != 255 && saved.option[i] >= cps1_dip_option_count(i)) return;
    set_dips(&saved);
}

static bool edit_dips(int game)
{
    const char *id = cps1_game_id(game);
    DipProfile original = current_dips(), draft = original;
    if (!draft.count) return false;
    int row = 0;
    for (;;) {
        Text groups[MAX_DIP+1];
        for (int i = 0; i < draft.count; ++i) {
            const char *name = translation(cps1_dip_name(i));
            const char *value = translation(cps1_dip_option_name(i, draft.option[i]));
            snprintf(groups[i], LABEL, "%.18s   %.15s", name, value);
        }
        strcpy(groups[draft.count], "Retablir les DIP par defaut");
        row = pick("INTERRUPTEURS DE LA BORNE", cps1_game_title(game), groups,
                   draft.count + 1, row, true, "Sauver les DIP redemarre la partie.");
        if (row == SAVE) {
            if (!memcmp(original.option, draft.option, draft.count)) return false;
            const Text choices[] = {"Enregistrer et redemarrer", "Continuer les reglages"};
            int answer = pick("REDEMARRER LA BORNE ?", "La partie en cours sera terminee.", choices,
                              2, 1, false, "Les nouveaux DIP seront memorises.");
            if (answer != 0) { row = 0; continue; }
            while (!profile_write(id, "dip", &draft, sizeof(draft))) if (!save_problem()) return false;
            set_dips(&draft);
            cps1_reset();
            return true;
        }
        if (row < 0) return false;
        if (row == draft.count) {
            for (int i = 0; i < draft.count; ++i)
                draft.option[i] = (uint8_t)cps1_dip_default(i);
            continue;
        }
        int n = cps1_dip_option_count(row);
        if (n <= 0 || n > 255) continue;
        Text choices[n];
        for (int i = 0; i < n; ++i)
            snprintf(choices[i], LABEL, "%s", translation(cps1_dip_option_name(row,i)));
        int value = pick("REGLER UN INTERRUPTEUR", translation(cps1_dip_name(row)),
                         choices, n, draft.option[row], false, "Choisis une position.");
        if (value >= 0) draft.option[row] = (uint8_t)value;
    }
}

bool settings_edit(int game, Bindings *bindings)
{
    const Text choices[] = {"Touches du joueur", "DIP switches de la borne", "Reprendre le jeu"};
    int row = 0;
    while (aptMainLoop()) {
        row = pick("REGLAGES DU JEU", cps1_game_title(game), choices, 3, row, false,
                   "Les reglages sont propres a ce jeu.");
        if (row == 0) edit_buttons(game, bindings);
        else if (row == 1) { if (edit_dips(game)) return true; }
        else break;
    }
    return false;
}
