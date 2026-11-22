#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    CPS1_WIDTH = 384, CPS1_HEIGHT = 224,
    CPS1_SAMPLE_RATE = 22050, CPS1_MAX_SAMPLES = 370,
    CPS1_FPS_NUM = 5963, CPS1_FPS_DEN = 100
};
enum {
    CPS1_COIN = 1u << 0, CPS1_START = 1u << 1,
    CPS1_UP = 1u << 2, CPS1_DOWN = 1u << 3,
    CPS1_LEFT = 1u << 4, CPS1_RIGHT = 1u << 5,
    CPS1_ATTACK = 1u << 6, CPS1_JUMP = 1u << 7,
    CPS1_BUTTON3 = 1u << 8, CPS1_BUTTON4 = 1u << 9,
    CPS1_BUTTON5 = 1u << 10, CPS1_BUTTON6 = 1u << 11
};

/* Catalogue pointers refer to static data. Invalid indices return NULL/zero. */
int cps1_game_count(void);
const char *cps1_game_id(int game);
const char *cps1_game_title(int game);
const char *cps1_game_parent(int game);
int cps1_game_buttons(int game);
int cps1_game_vertical(int game);
int cps1_game_chip(int game, unsigned chip, const char **name, uint32_t *bytes, uint32_t *crc);
/* -1 automatically identifies the game by its program and data chips. */
int cps1_open_game(int game, const char *path);
int cps1_current_game(void);
/* Switches of the open board. Group and option indices start at zero. */
int cps1_dip_count(void);
const char *cps1_dip_name(int group);
int cps1_dip_option_count(int group);
const char *cps1_dip_option_name(int group, int option);
int cps1_dip_get(int group);
int cps1_dip_default(int group);
int cps1_dip_set(int group, int option);
void cps1_dip_defaults(void);
/* Pulse the emulated board's reset input on the next frame. Keeps DIP settings. */
void cps1_reset(void);
/* Opens the supplied ZIP read-only. Returns zero on success. */
int cps1_open(const char *path);
int cps1_missing_samples(void);
const char *cps1_error(void);
/* One CPS1 video period. A null video skips rendering. For PCM output,
 * supply space for CPS1_MAX_SAMPLES stereo pairs. Returns stereo pairs
 * produced, or -1 on error. Either output buffer may be null. */
int cps1_frame(uint32_t buttons, uint16_t *rgb565, int16_t *stereo);
void cps1_close(void);

#ifdef __cplusplus
}
#endif
