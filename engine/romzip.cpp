#include "ui_language.h"
#include "romzip.h"
#include "cps1.h"
#include "unzip.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {
struct Key {
    uint32_t size, crc;
    bool operator==(const Key &other) const { return size == other.size && crc == other.crc; }
};

struct Directory {
    std::string path;
    std::vector<Key> entries;
    bool valid;
    explicit Directory(const std::string &name) : path(name), valid(false) {
        unzFile zip = unzOpen(path.c_str());
        if (!zip) return;
        int rc = unzGoToFirstFile(zip);
        while (rc == UNZ_OK) {
            unz_file_info info;
            if (unzGetCurrentFileInfo(zip, &info, NULL, 0, NULL, 0, NULL, 0) != UNZ_OK) break;
            entries.push_back({(uint32_t)info.uncompressed_size, (uint32_t)info.crc});
            rc = unzGoToNextFile(zip);
        }
        valid = rc == UNZ_END_OF_LIST_OF_FILE;
        unzClose(zip);
    }
    bool has(const Key &key) const {
        return valid && std::find(entries.begin(), entries.end(), key) != entries.end();
    }
};

struct FreeBytes { void operator()(unsigned char *p) const { std::free(p); } };
struct Chip {
    Key key;
    const char *name;
    int directory = -1;
    std::unique_ptr<unsigned char, FreeBytes> data;
};
std::vector<Chip> loaded;
bool missingSamples;

std::vector<Chip> manifest(int game) {
    std::vector<Chip> chips;
    for (unsigned i = 0;; ++i) {
        Chip chip;
        if (cps1_game_chip(game, i, &chip.name, &chip.key.size, &chip.key.crc)) break;
        chips.push_back(std::move(chip));
    }
    return chips;
}

int find_id(const std::string &id) {
    for (int i = 0; i < cps1_game_count(); ++i)
        if (id == cps1_game_id(i)) return i;
    return -1;
}

std::string set_name(const std::string &path) {
    size_t start = path.find_last_of("/\\");
    std::string name = path.substr(start == std::string::npos ? 0 : start + 1);
    size_t extension = name.find_last_of('.');
    if (extension != std::string::npos) name.resize(extension);
    for (char &c : name) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return name;
}

std::string peer_path(const std::string &path, const char *id) {
    size_t start = path.find_last_of("/\\");
    return path.substr(0, start == std::string::npos ? 0 : start + 1) + id + ".zip";
}

// Visit only the parent chain. Unrelated games in the ROM folder never
// silently contribute chips. Cache both present and missing directories.
int directory_for(std::vector<Directory> &archives, const std::string &path) {
    for (size_t i = 0; i < archives.size(); ++i)
        if (archives[i].path == path) return (int)i;
    archives.emplace_back(path);
    return (int)archives.size() - 1;
}

struct Candidate {
    int game, present, required;
    bool named;
    bool operator<(const Candidate &b) const {
        if (named != b.named) return named;
        int lhs = present * b.required, rhs = b.present * required;
        if (lhs != rhs) return lhs > rhs;
        if (present != b.present) return present > b.present;
        return game < b.game;
    }
};

// A renamed split archive needs a chip specific to this clone. A single
// common sound or graphics chip does not identify a revision.
bool has_clone_evidence(const Directory &primary, const std::vector<Chip> &chips, int parent) {
    if (parent < 0) return false;
    std::vector<Chip> inherited = manifest(parent);
    for (const Chip &chip : chips) {
        if (!chip.key.size || !primary.has(chip.key)) continue;
        bool shared = std::any_of(inherited.begin(), inherited.end(),
            [&](const Chip &item) { return item.key == chip.key; });
        if (!shared) return true;
    }
    return false;
}

int assign(std::vector<Chip> &chips, const Directory &archive, int directory) {
    int missing = 0;
    for (Chip &chip : chips) {
        if (!chip.key.size || !chip.key.crc || chip.directory >= 0 || chip.data) continue;
        if (archive.has(chip.key)) chip.directory = directory;
        else ++missing;
    }
    return missing;
}

bool extract(unzFile zip, Chip &chip) {
    chip.data.reset(static_cast<unsigned char *>(std::malloc(chip.key.size)));
    if (!chip.data || unzOpenCurrentFile(zip) != UNZ_OK) return false;
    unsigned done = 0;
    while (done < chip.key.size) {
        int n = unzReadCurrentFile(zip, chip.data.get() + done, chip.key.size - done);
        if (n <= 0) break;
        done += (unsigned)n;
    }
    int rc = unzCloseCurrentFile(zip);
    return done == chip.key.size && rc == UNZ_OK;
}

// Exact alternate layouts present in older arcade archives. Both the source
// chip and the assembled bytes must pass CRC before they reach a driver.
bool readVerified(const std::string &path, Key source, Chip &part) {
    unzFile z = unzOpen(path.c_str());
    if (!z) return false;
    bool ok = false;
    for (int it = unzGoToFirstFile(z); it == UNZ_OK; it = unzGoToNextFile(z)) {
        unz_file_info info;
        if (unzGetCurrentFileInfo(z, &info, nullptr, 0, nullptr, 0, nullptr, 0) != UNZ_OK) break;
        if (info.crc == source.crc && info.uncompressed_size == source.size) {
            part.key = source;
            ok = extract(z, part);
            break;
        }
    }
    unzClose(z);
    return ok;
}

void recoverTurboChips(std::vector<Chip> &chips, const std::string &path, bool doubled) {
    struct Lanes { uint32_t expected, first, second; unsigned lane; };
    static const Lanes lanes[] = {
        {0x3a85a275,0x994bfa58,0xc1befaa8,0}, {0xecdb083b,0x994bfa58,0xc1befaa8,1},
        {0x9156472f,0x3e66ad9d,0x0627c831,0}, {0x8fea8384,0x3e66ad9d,0x0627c831,1}
    };
    static const uint32_t mirrors[][2] = {
        {0x158635ca,0x47468198}, {0x882cd1c4,0x47d34191},
        {0x1073b7b6,0x5507bac9}, {0xdb7e1f72,0x7755a20a}
    };
    for (Chip &c : chips) {
        if (c.directory >= 0 || !c.key.size || c.data) continue;
        for (const auto &layout : lanes) {
            if (layout.expected != c.key.crc || c.key.size != 0x20000) continue;
            Chip left, right;
            auto donor = peer_path(path, "sf2");
            if (!readVerified(donor, {0x80000,layout.first}, left) ||
                !readVerified(donor, {0x80000,layout.second}, right)) break;
            c.data.reset(static_cast<unsigned char *>(std::malloc(c.key.size)));
            if (!c.data) break;
            for (unsigned i=0; i<0x10000; ++i) {
                c.data.get()[i] = left.data.get()[2*i+layout.lane];
                c.data.get()[i+0x10000] = right.data.get()[2*i+layout.lane];
            }
        }
        if (doubled) for (const auto &pair : mirrors) {
            if (c.key.crc != pair[0]) continue;
            Chip mirror;
            if (readVerified(path, {2*c.key.size,pair[1]}, mirror) &&
                !std::memcmp(mirror.data.get(),mirror.data.get()+c.key.size,c.key.size))
                c.data = std::move(mirror.data);
        }
        if (c.data && crc32(0, c.data.get(), c.key.size) != c.key.crc) c.data.reset();
    }
}

bool read_chips(std::vector<Chip> &chips, const std::vector<Directory> &archives,
                char *error, size_t capacity) {
    for (size_t i = 0; i < archives.size(); ++i) {
        int remaining = (int)std::count_if(chips.begin(), chips.end(),
            [&](const Chip &c) { return c.directory == (int)i; });
        if (!remaining) continue;
        unzFile zip = unzOpen(archives[i].path.c_str());
        int rc = zip ? unzGoToFirstFile(zip) : UNZ_ERRNO;
        while (remaining && rc == UNZ_OK) {
            unz_file_info info;
            if (unzGetCurrentFileInfo(zip, &info, NULL, 0, NULL, 0, NULL, 0) != UNZ_OK) break;
            for (Chip &c : chips) {
                if (c.directory != (int)i || c.data || c.key.crc != info.crc ||
                    c.key.size != info.uncompressed_size) continue;
                if (!extract(zip, c)) {
                    std::snprintf(error, capacity, ui_text("Lecture/CRC impossible : %s (%08lx).", "Read/CRC failed: %s (%08lx)."),
                                  c.name, (unsigned long)c.key.crc);
                    unzClose(zip);
                    return false;
                }
                --remaining;
            }
            if (remaining) rc = unzGoToNextFile(zip);
        }
        if (zip) unzClose(zip);
        if (remaining) {
            std::snprintf(error, capacity, ui_text("ZIP illisible pendant le chargement.", "Could not read ZIP during loading."));
            return false;
        }
    }
    return true;
}
}

int romzip_prepare(const char *path, int requested, char *error, size_t capacity) {
    romzip_release();
    missingSamples = false;
    if (!error || !capacity) return -1;
    error[0] = 0;
    if (!path || requested < -1 || requested >= cps1_game_count()) {
        std::snprintf(error, capacity, ui_text("Selection de jeu invalide.", "Invalid game selection."));
        return -1;
    }
    std::vector<Directory> archives;
    archives.emplace_back(path);
    if (!archives[0].valid) {
        std::snprintf(error, capacity, ui_text("Impossible de lire le repertoire du ZIP.", "Could not read the ZIP directory."));
        return -1;
    }
    int named = find_id(set_name(path));
    std::vector<Candidate> choices;
    for (int game = 0; game < cps1_game_count(); ++game) {
        if (requested >= 0 && requested != game) continue;
        int present = 0, required = 0;
        for (const Chip &c : manifest(game)) {
            if (!c.key.size || !c.key.crc) continue;
            ++required;
            if (archives[0].has(c.key)) ++present;
        }
        if (required && (present || requested == game || named == game))
            choices.push_back({game, present, required, game == named});
    }
    std::sort(choices.begin(), choices.end());
    for (const Candidate &pick : choices) {
        std::vector<Chip> chips = manifest(pick.game);
        int missing = assign(chips, archives[0], 0);
        const char *parent = cps1_game_parent(pick.game);
        const bool inspect_family = (pick.present || pick.named) && parent && *parent &&
            (pick.named || requested >= 0 || has_clone_evidence(archives[0], chips, find_id(parent)));
        if (missing && inspect_family) {
            std::vector<std::string> visited;
            while (missing && parent && *parent &&
                   std::find(visited.begin(), visited.end(), parent) == visited.end()) {
                visited.emplace_back(parent);
                int idx = directory_for(archives, peer_path(path, parent));
                missing = assign(chips, archives[idx], idx);
                int parent_game = find_id(parent);
                parent = parent_game < 0 ? NULL : cps1_game_parent(parent_game);
            }
        }
        const std::string gameId = cps1_game_id(pick.game);
        if (missing && pick.named && (gameId == "sf2toryu" || gameId == "sf2turyu" || gameId == "sf2turyu1")) {
            recoverTurboChips(chips,path,gameId == "sf2turyu");
            missing = assign(chips,archives[0],0);
        }
        if (missing && pick.named && gameId == "dinopic2") {
            int peer = directory_for(archives, peer_path(path, "dinopic"));
            missing = assign(chips,archives[peer],peer);
        }
        bool preview = false;
        if (missing && pick.named && gameId == "wofh") {
            // This board uses the sound chips catalogued with sgyxz.
            int donor = directory_for(archives, peer_path(path, "sgyxz"));
            missing = assign(chips, archives[donor], donor);
            for (Chip &c : chips) {
                if (c.directory >= 0 || c.key.crc != 0x210c376f || c.key.size != 0x10000) continue;
                Chip wide;
                if (readVerified(peer_path(path,"wofhfh"), {0x20000,0x86fe8a97}, wide) &&
                    crc32(0, wide.data.get(), c.key.size) == c.key.crc) {
                    c.data = std::move(wide.data);
                    --missing;
                }
            }
            if (missing == 1) for (Chip &c : chips) {
                if (c.directory >= 0 || c.key.crc != 0xc15ac0f2 || c.key.size != 0x40000) continue;
                // Empty OKI sample table. The menu asks before entering this preview.
                c.data.reset(static_cast<unsigned char *>(std::calloc(c.key.size, 1)));
                if (c.data) { --missing; preview = true; }
            }
        }
        if (!missing) {
            if (!read_chips(chips, archives, error, capacity)) return -1;
            loaded.swap(chips);
            missingSamples = preview;
            error[0] = 0;
            return pick.game;
        }
        if (!error[0]) {
            const char *parent_id = cps1_game_parent(pick.game);
            auto it = std::find_if(chips.begin(), chips.end(), [](const Chip &c) {
                return c.key.size && c.key.crc && c.directory < 0 && !c.data;
            });
            std::snprintf(error, capacity, ui_text("%s : puce %s (%08lx) absente.%s%s%s", "%s: missing chip %s (%08lx).%s%s%s"),
                          cps1_game_id(pick.game), it->name, (unsigned long)it->key.crc,
                          parent_id && *parent_id ? ui_text(" Parent : ", " Parent: ") : "",
                          parent_id && *parent_id ? parent_id : "",
                          parent_id && *parent_id ? ".zip" : "");
        }
    }
    if (!error[0]) std::snprintf(error, capacity, ui_text("Jeu CPS1 inconnu ou revision non prise en charge.", "Unknown CPS1 game or unsupported revision."));
    return -1;
}

int romzip_missing_samples() { return missingSamples; }

int romzip_load(unsigned char *destination, int *written, int index) {
    if (index < 0 || (size_t)index >= loaded.size() || !destination) return 1;
    const Chip &chip = loaded[index];
    if (!chip.data) return 1;
    std::memcpy(destination, chip.data.get(), chip.key.size);
    if (written) *written = (int)chip.key.size;
    return 0;
}

void romzip_release() {
    loaded.clear();
}
