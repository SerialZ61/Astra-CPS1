#pragma once
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char *path;
    int game;
} RomEntry;

typedef struct {
    RomEntry *entries;
    size_t count, capacity;
    bool incomplete;
} RomLibrary;

void library_scan(RomLibrary *library);
void library_free(RomLibrary *library);
int library_game_for_path(const char *path);
const char *library_filename(const char *path);
