#pragma once
#include <stdint.h>
struct BurnInputInfo;
struct BurnDIPInfo;
struct AstraExtraApi {
    int (*vertical)(int);
    int (*chip)(int, unsigned, const char **, uint32_t *, uint32_t *);
    int (*open)(int, int (*)(unsigned char *, int *, int), int, int);
    void (*close)();
    int (*frame)(uint16_t *, int16_t *, int);
    void (*reset)(bool);
    int (*input)(BurnInputInfo *, unsigned);
    int (*dip)(BurnDIPInfo *, unsigned);
};
extern "C" const AstraExtraApi astra_extra_api;
