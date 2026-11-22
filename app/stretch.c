#include "stretch.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

enum { OVERLAP = 64 };

unsigned stretch_audio(AudioStretch *s, const int16_t *input, unsigned pairs,
                       unsigned extra, int16_t *output)
{
    if (!pairs || pairs > CPS1_MAX_SAMPLES) return 0;
    if (!extra) {
        memcpy(output, input, pairs * 2 * sizeof(*input));
        memcpy(s->history, input, pairs * 2 * sizeof(*input));
        s->previous = pairs;
        s->debt = 0;
        return pairs;
    }
    int16_t *current = s->history + 2 * s->previous;
    memcpy(current, input, pairs * 2 * sizeof(*input));
    int wanted = (int)extra + s->debt;
    int best = 0, cut = 0;
    int repeats = wanted > (int)pairs ? 2 : 1;
    int target = wanted / repeats;
    if (pairs >= 2 * OVERLAP && wanted >= 24) {
        int low = target - 56, high = target + 56;
        if (low < 20) low = 20;
        if (repeats > 1 && low < OVERLAP) low = OVERLAP;
        if (high > (int)pairs) high = pairs;
        if (low > high) low = high;
        unsigned best_score = UINT_MAX;
        for (int region = 1; region <= 3; ++region) {
            int start = ((int)pairs - OVERLAP) * region / 4;
            for (int lag = low; lag <= high && lag <= (int)s->previous + start; ++lag) {
                unsigned score = 8u * (unsigned)abs(lag - target);
                for (int i = 0; i < OVERLAP; i += 4) {
                    int at = 2 * (start+i);
                    score += abs(current[at] - current[at-2*lag]);
                    score += abs(current[at+1] - current[at-2*lag+1]);
                }
                if (score < best_score) {
                    best_score = score; best = lag; cut = start;
                }
            }
        }
    }
    if (!best) memcpy(output, input, pairs * 2 * sizeof(*input));
    else {
        memcpy(output, input, cut * 2 * sizeof(*input));
        for (int pass = 0; pass < repeats; ++pass) {
            int16_t *segment = output + 2 * pass * best;
            for (int i = 0; i < OVERLAP; ++i) {
                for (int ch = 0; ch < 2; ++ch) {
                    int at = 2 * (cut+i) + ch;
                    segment[at] = ((OVERLAP-i)*current[at] + i*current[at-2*best]) / OVERLAP;
                }
            }
            int end = cut + OVERLAP;
            int length = pass + 1 == repeats ? (int)pairs + best - end : best - OVERLAP;
            memcpy(segment + 2*end, current + 2*(end-best), length * 2 * sizeof(*input));
        }
    }
    s->debt = wanted - repeats * best;
    if (s->debt > (int)pairs) s->debt = pairs;
    memmove(s->history, current, pairs * 2 * sizeof(*input));
    s->previous = pairs;
    return pairs + repeats * best;
}
