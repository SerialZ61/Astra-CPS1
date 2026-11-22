#include "library.h"
#include "cps1.h"
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

const char *library_filename(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

int library_game_for_path(const char *path)
{
    const char *file = library_filename(path);
    const char *dot = strrchr(file, '.');
    if (!dot || strcasecmp(dot, ".zip")) return -1;
    size_t length = (size_t)(dot - file);
    // The older USA 900613 archive convention appears on this collection.
    // This label is only a menu hint; opening still resolves actual chip CRCs.
    if (!strcasecmp(file, "ffightub.zip")) {
        file = "ffightua";
        length = strlen(file);
    }
    for (int i = 0; i < cps1_game_count(); ++i) {
        const char *id = cps1_game_id(i);
        if (strlen(id) == length && !strncasecmp(file, id, length)) return i;
    }
    return -1;
}

static bool add_file(RomLibrary *library, const char *dir, const char *name, int game)
{
    // Keep the installation directory first when several frontends share a set.
    for (size_t i = 0; i < library->count; ++i) {
        if (!strcasecmp(library_filename(library->entries[i].path), name)) return true;
    }
    if (library->count == library->capacity) {
        size_t capacity = library->capacity ? library->capacity * 2 : 16;
        RomEntry *entries = realloc(library->entries, capacity * sizeof(*entries));
        if (!entries) return false;
        library->entries = entries;
        library->capacity = capacity;
    }
    size_t dirlen = strlen(dir), namelen = strlen(name);
    char *path = malloc(dirlen + namelen + 2);
    if (!path) return false;
    memcpy(path, dir, dirlen);
    path[dirlen] = '/';
    memcpy(path + dirlen + 1, name, namelen + 1);
    library->entries[library->count++] = (RomEntry){ path, game };
    return true;
}

static int compare_games(const void *first, const void *second)
{
    const RomEntry *a = first, *b = second;
    if ((a->game < 0) != (b->game < 0)) return a->game < 0 ? 1 : -1;
    int cmp = a->game < 0 ? 0 : strcasecmp(cps1_game_title(a->game), cps1_game_title(b->game));
    return cmp ? cmp : strcasecmp(library_filename(a->path), library_filename(b->path));
}

void library_scan(RomLibrary *library)
{
    static const struct { const char *path; bool dedicated; } dirs[] = {
        { "sdmc:/3ds/AstraCPS1/roms", true },
        { "sdmc:/roms/arcade/cps1", true },
        { "sdmc:/roms", false },
        { "sdmc:/roms/arcade/fba2012", false },
    };
    library_free(library);
    for (size_t i = 0; i < sizeof(dirs) / sizeof(*dirs); ++i) {
        DIR *dir = opendir(dirs[i].path);
        if (!dir) continue;
        struct dirent *entry;
        while ((entry = readdir(dir))) {
            const char *name = entry->d_name;
            const char *ext = strrchr(name, '.');
            if (*name == '.' || !ext || strcasecmp(ext, ".zip")) continue;
#ifdef DT_DIR
            if (entry->d_type == DT_DIR) continue;
#endif
            int game = library_game_for_path(name);
            if (game < 0 && !dirs[i].dedicated) continue;
            if (!add_file(library, dirs[i].path, name, game)) {
                library->incomplete = true;
                break;
            }
        }
        closedir(dir);
        if (library->incomplete) break;
    }
    if (library->count > 1)
        qsort(library->entries, library->count, sizeof(*library->entries), compare_games);
}

void library_free(RomLibrary *library)
{
    for (size_t i = 0; i < library->count; ++i) free(library->entries[i].path);
    free(library->entries);
    *library = (RomLibrary){0};
}
