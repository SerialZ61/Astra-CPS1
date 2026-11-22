#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
extern bool ui_english;
static inline const char *ui_text(const char *fr, const char *en)
{
    return ui_english ? en : fr;
}
void language_load(void);
bool language_save(bool english);
#ifdef __cplusplus
}
#endif
