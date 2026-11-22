#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "cps1.h"
#include "settings.h"
#include "display.h"
#include "library.h"
#include "menu.h"
#include "pacing.h"
#include "sound.h"

static int16_t pcm[CPS1_MAX_SAMPLES * 2];
static const char *const modes[] = { "Auto", "30 i/s", "1 sur 2", "Toutes" };
static bool native;
static bool continuous = true;
static enum cabinet_view view, preferred_portrait = VIEW_PORTRAIT;
static enum video_mode mode = VIDEO_AUTO;
static volatile bool clock_reset;

static void apt_event(APT_HookType event, void *unused)
{
    (void)unused;
    if (event == APTHOOK_ONRESTORE || event == APTHOOK_ONWAKEUP) clock_reset = true;
}

static void status_layout(int game, const Bindings *bindings)
{
    printf("\x1b[2J\x1b[1;1H\x1b[1;36mASTRA CPS1\x1b[0m  /  OLD 3DS   v0.6a\n");
    printf("%.38s\n\n", cps1_game_title(game));
    int actions = cps1_game_buttons(game);
    bool ffight = !strncmp(cps1_game_id(game), "ffight", 6);
    for (int i = 0; i < 6; i += 2) {
        printf("%-2s %-15.15s  %-2s %.15s\n",
               bindings_key_name(i), bindings_action_name(bindings->action[i], actions, ffight),
               bindings_key_name(i+1), bindings_action_name(bindings->action[i+1], actions, ffight));
    }
    printf("START  %.13s   SELECT  %.12s\n",
           bindings_action_name(bindings->action[6], actions, ffight),
           bindings_action_name(bindings->action[7], actions, ffight));
    printf("SELECT+START  Jeux   L+R+START  Quitter\n");
}

static void show_mode(bool paused, bool audio)
{
    printf("\x1b[12;1HCadence %-9s    Vue %-10s", modes[mode], view == VIEW_PORTRAIT ? "Portrait" : view == VIEW_DPAD_TOP ? "Croix haut" : view == VIEW_DPAD_BOTTOM ? "Croix bas" : native ? "384 x 224" : "4:3 arcade");
    printf("\x1b[13;1H%-9s  Ecran %s   %s     ", paused ? "En pause" : "En jeu",
           display_is_accelerated() ? "GPU" : "CPU", audio ? "DSP 22 050 Hz" : "Sans DSP");
    printf("\x1b[24;1H%-39s", audio ?
           (continuous ? "[ SON CONTINU ]  Toucher pour changer" : "[ SON ARCADE ]   Toucher pour changer") : "");
    printf("\x1b[28;1H\x1b[1;36m%-8s%-8s%-8s%-8s%s\x1b[0m",
           paused ? "REPRISE" : " PAUSE", "CADENCE", " FORMAT", " REGLER", "  JEUX");
    display_flush_console();
}

enum touch_action { TOUCH_NONE, TOUCH_PAUSE, TOUCH_RATE, TOUCH_FORMAT, TOUCH_SETTINGS, TOUCH_GAMES, TOUCH_AUDIO };

static enum touch_action touch_control(u32 down)
{
    if (!(down & KEY_TOUCH)) return TOUCH_NONE;
    touchPosition tap;
    hidTouchRead(&tap);
    if (tap.py >= 180 && tap.py < 204) return TOUCH_AUDIO;
    return tap.py >= 216 ? (enum touch_action)(TOUCH_PAUSE + tap.px / 64) : TOUCH_NONE;
}

// True returns to the ROM list. False exits to the Homebrew Launcher.
static bool play_game(const char *path, bool audio)
{
    int game = cps1_current_game();
    view = cps1_game_vertical(game) ? preferred_portrait : VIEW_HORIZONTAL;
    display_set_view(native, view);
    Bindings bindings;
    bindings_load(&bindings, cps1_game_id(game), cps1_game_buttons(game));
    settings_load_dips(game);
    bool paused = false, has_picture = false;
    unsigned measured_frames = 0, measured_draws = 0;
    uint32_t screen_start = display_submissions();
    unsigned underruns = sound_underruns(), drops = sound_drops();
    const u64 ticks_per_frame = (u64)SYSCLOCK_ARM11 * CPS1_FPS_DEN / CPS1_FPS_NUM;
    u64 deadline, window_start, drawn_ticks = 0, plain_ticks = 0;
    char last_stats[320] = "Mesure en cours...";
    FramePacer pacer;
    pacing_reset(&pacer, ticks_per_frame);
    status_layout(game, &bindings);
    printf("\x1b[16;1H%s", last_stats);
    show_mode(paused, audio);
    clock_reset = false;
    deadline = window_start = svcGetSystemTick();

    while (aptMainLoop()) {
        display_service();
        hidScanInput();
        u32 held = hidKeysHeld(), down = hidKeysDown();
        const u32 exit_chord = KEY_L | KEY_R | KEY_START;
        const u32 menu_chord = KEY_SELECT | KEY_START;
        enum touch_action tap = touch_control(down);
        if ((held & exit_chord) == exit_chord) return false;
        if ((held & menu_chord) == menu_chord || tap == TOUCH_GAMES) return true;
        if (tap == TOUCH_AUDIO && audio) {
            continuous = !continuous;
            clock_reset = true;
            underruns = sound_underruns();
            drops = sound_drops();
            strcpy(last_stats, "Mesure en cours...");
            status_layout(game, &bindings);
            printf("\x1b[16;1H%s", last_stats);
            show_mode(paused, audio);
        }

        if (tap == TOUCH_SETTINGS) {
            sound_reset();
            if (settings_edit(game, &bindings)) {
                paused = false;
                strcpy(last_stats, "Mesure en cours...");
                pacing_reset(&pacer, ticks_per_frame);
            }
            clock_reset = true;
            status_layout(game, &bindings);
            printf("\x1b[16;1H%s", last_stats);
            show_mode(paused, audio);
            continue;
        }
        if (tap == TOUCH_RATE || bindings_pressed(&bindings, BIND_RATE, down)) {
            mode = (mode + 1) % VIDEO_MODE_COUNT;
            pacing_reset(&pacer, ticks_per_frame);
            clock_reset = true;
            underruns = sound_underruns();
            drops = sound_drops();
            strcpy(last_stats, "Mesure en cours...");
            status_layout(game, &bindings);
            printf("\x1b[16;1H%s", last_stats);
            show_mode(paused, audio);
        }
        if (tap == TOUCH_FORMAT || bindings_pressed(&bindings, BIND_FORMAT, down)) {
            if (view == VIEW_HORIZONTAL) native = !native;
            else {
                preferred_portrait = view == VIEW_DPAD_BOTTOM ? VIEW_PORTRAIT : view+1;
                view = preferred_portrait;
            }
            display_set_view(native, view);
            if (paused && has_picture) display_redraw();
            show_mode(paused, audio);
        }
        if (tap == TOUCH_PAUSE || bindings_pressed(&bindings, BIND_PAUSE, down)) {
            paused = !paused;
            clock_reset = true;
            show_mode(paused, audio);
        }
        if (clock_reset) {
            clock_reset = false;
            sound_reset();
            deadline = window_start = svcGetSystemTick();
            screen_start = display_submissions();
            measured_frames = measured_draws = 0;
            drawn_ticks = plain_ticks = 0;
        }
        if (paused) {
            gspWaitForVBlank();
            continue;
        }

        if (audio) sound_wait_for_slot();
        u64 now = svcGetSystemTick();
        uint16_t *pixels = pacing_wants_draw(&pacer, mode, now) ? display_acquire() : NULL;
        int pairs = cps1_frame(display_directions(bindings_buttons(&bindings, held), view), pixels, audio ? pcm : NULL);
        if (pairs < 0) {
            sound_reset();
            menu_error(path, cps1_error());
            return true;
        }
        if (audio) sound_submit(pcm, (unsigned)pairs, continuous);
        if (pixels) {
            display_present(pixels);
            has_picture = true;
            ++measured_draws;
        }
        uint32_t elapsed = (uint32_t)(svcGetSystemTick() - now);
        pacing_record(&pacer, pixels != NULL, elapsed);
        if (pixels) drawn_ticks += elapsed;
        else plain_ticks += elapsed;
        ++measured_frames;
        deadline += ticks_per_frame;
        now = svcGetSystemTick();
        if (now - window_start >= 2 * (u64)SYSCLOCK_ARM11) {
            float seconds = (float)(now - window_start) / SYSCLOCK_ARM11;
            float rate = measured_frames / seconds;
            const float tick_ms = 1000.0f / SYSCLOCK_ARM11;
            uint32_t screen_now = display_submissions();
            unsigned plain_frames = measured_frames - measured_draws;
            char sound_stats[64] = "";
            if (audio) snprintf(sound_stats, sizeof(sound_stats), "Audio vides %u / pleins %u    ",
                                sound_underruns() - underruns, sound_drops() - drops);
            snprintf(last_stats, sizeof(last_stats),
                     "Vitesse       %5.1f %%    \nEmulation     %5.1f images/s\n"
                     "Affichage     %5.1f images/s\nDessinees     %5.1f images/s\n"
                     "Avec image    %5.1f ms\nSans image    %5.1f ms\n%s",
                     rate * 100 * CPS1_FPS_DEN / CPS1_FPS_NUM, rate,
                     (screen_now - screen_start) / seconds, measured_draws / seconds,
                     measured_draws ? drawn_ticks * tick_ms / measured_draws : 0.0f,
                     plain_frames ? plain_ticks * tick_ms / plain_frames : 0.0f, sound_stats);
            printf("\x1b[16;1H%s", last_stats);
            display_flush_console();
            screen_start = screen_now;
            window_start = now;
            measured_frames = measured_draws = 0;
            drawn_ticks = plain_ticks = 0;
        }
        now = svcGetSystemTick();
        if (now < deadline) {
            if (audio && sound_needs_refill()) {
                u64 slack = deadline - now;
                u64 advance = ticks_per_frame / 8;
                deadline -= slack < advance ? slack : advance;
            }
            display_wait_until(deadline);
        }
        else if (now - deadline > 2 * ticks_per_frame) deadline = now;
    }
    return false;
}

int main(int argc, char **argv)
{
    display_init();
    RomLibrary library = {0};
    library_scan(&library);
    int selected = library.count == 1 ? 0 : -1;
    const char *external = argc > 1 ? argv[1] : NULL;
    const char *ext = external ? strrchr(external, '.') : NULL;
    if (!ext || strcasecmp(ext, ".zip")) external = NULL;
    int last = 0;
    bool sound_tried = false, audio = false;
    aptHookCookie hook;
    aptHook(&hook, apt_event, NULL);
    while (aptMainLoop()) {
        const char *path;
        if (external) {
            path = external;
            external = NULL;
        } else {
            if (selected < 0) selected = menu_choose(&library, last);
            if (selected < 0) break;
            path = library.entries[selected].path;
            last = selected;
            selected = -1;
        }
        printf("\x1b[2J\x1b[1;1HASTRA CPS1\n\nChargement...\n%.39s", library_filename(path));
        display_flush_console();
        if (cps1_open(path) != 0) {
            menu_error(path, cps1_error());
            selected = -1;
            continue;
        }
        if (cps1_missing_samples() && !menu_confirm_missing_samples()) {
            cps1_close();
            continue;
        }
        if (!sound_tried) {
            audio = sound_open();
            sound_tried = true;
        }
        bool again = play_game(path, audio);
        sound_reset();
        cps1_close();
        if (!again) break;
        selected = -1;
    }
    aptUnhook(&hook);
    sound_close();
    library_free(&library);
    display_exit();
    gfxExit();
    return 0;
}
