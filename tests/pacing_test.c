#include <assert.h>
#include <stdint.h>
#include "../app/pacing.h"

// Host test: compile this file and app/pacing.c with a C11 compiler.
static unsigned simulate(enum video_mode mode, unsigned count, uint32_t drawn,
                         uint32_t skipped)
{
    FramePacer pacer;
    pacing_reset(&pacer, 16770);
    unsigned draws = 0;
    uint64_t now = 100;
    for (unsigned i = 0; i < count; ++i) {
        bool draw = pacing_wants_draw(&pacer, mode, now);
        if (draw) ++draws;
        pacing_record(&pacer, draw, draw ? drawn : skipped);
        now += pacer.period;
    }
    return draws;
}

int main(void)
{
    // The user's first timing window fits under one full arcade period, even
    // before the very first skipped sample is available to the estimator.
    assert(simulate(VIDEO_AUTO, 120, 11700, 7700) == 120);
    assert(simulate(VIDEO_ALL, 120, 23000, 7700) == 120);
    assert(simulate(VIDEO_HALF, 120, 11700, 7700) == 60);
    assert(simulate(VIDEO_30, 120, 11700, 7700) == 60);
    assert(simulate(VIDEO_30, 120, 38000, 22100) == 60);
    // At 23 ms with video / 7.7 ms without, the sustainable draw fraction
    // is near 56%, including the scheduler margin reserved by Auto.
    unsigned draws = simulate(VIDEO_AUTO, 1000, 23000, 7700);
    assert(draws >= 553 && draws <= 565);
    return 0;
}
