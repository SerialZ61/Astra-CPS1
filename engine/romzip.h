#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Inspect the first ZIP, resolve its set and verify every required chip.
 * requested_game == -1 lets the resolver select by contents. The callback
 * stays valid until romzip_release(), including repeated loads of one chip. */
int romzip_prepare(const char *path, int requested_game, char *error, size_t capacity);
int romzip_load(unsigned char *destination, int *written, int chip);
int romzip_missing_samples(void);
void romzip_release();

#ifdef __cplusplus
}
#endif
