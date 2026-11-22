#include <3ds.h>
#include "bindings.h"
#include "ui_language.h"

bool ui_english;

void language_load(void)
{
    uint8_t saved;
    if (profile_read("interface", "lang", &saved, sizeof(saved)) && saved <= 1) {
        ui_english = saved != 0;
        return;
    }
    u8 system_language = CFG_LANGUAGE_EN;
    if (R_SUCCEEDED(cfguInit())) {
        CFGU_GetSystemLanguage(&system_language);
        cfguExit();
    }
    ui_english = system_language != CFG_LANGUAGE_FR;
}

bool language_save(bool english)
{
    uint8_t saved = english ? 1 : 0;
    if (!profile_write("interface", "lang", &saved, sizeof(saved)))
        return false;
    ui_english = english;
    return true;
}
