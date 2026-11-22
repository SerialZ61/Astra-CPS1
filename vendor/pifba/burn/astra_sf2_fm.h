/* Block rendering for the YM2151 algorithms used in World Warrior.
 * A Z80 write always ends the current mixer segment.
 * The reference mixer handles register combinations outside these conditions.
 */
enum { FM_BLOCK = 64 };
static int sf2_music_enabled;
void AstraSf2Music(int enabled) { sf2_music_enabled = enabled; }

static inline int sf2_wave(unsigned attenuation, unsigned phase)
{
    unsigned index = attenuation + sin_tab[phase & SIN_MASK];
    return index < TL_TAB_LEN ? tl_tab[index] : 0;
}

/* Each operator has its own envelope. Values at consecutive ticks depend
 * only on that operator and the common envelope counter, so we can calculate
 * them together without changing when a decay or release becomes audible. */
static int sf2_levels(YM2151Operator *op, unsigned before, const unsigned *ticks,
                      int count, unsigned *level)
{
    unsigned constant = (op->tl + op->volume) << 3;
    if (op->state == EG_OFF || (op->state == EG_SUS && op->eg_sel_d2r == 18*RATE_STEPS &&
                               op->volume < MAX_ATT_INDEX))
        return (int)constant;
    unsigned shift = op->state == EG_ATT ? op->eg_sh_ar : op->state == EG_DEC ?
        op->eg_sh_d1r : op->state == EG_SUS ? op->eg_sh_d2r : op->eg_sh_rr;
    if ((before >> shift) == (ticks[count-1] >> shift)) return (int)constant;
    for (int i = 0; i < count; ++i) {
        while (before != ticks[i]) advance_envelope_at(op, ++before);
        level[i] = (op->tl + op->volume) << 3;
    }
    return -1;
}

static inline void sf2_voice(YM2151Operator *op, int count, unsigned before,
                             const unsigned *ticks, const int *input, int *output)
{
    unsigned level[FM_BLOCK];
    int fixed = sf2_levels(op, before, ticks, count, level);
    uint32_t phase = op->phase, step = op->freq;
    if (fixed >= TL_TAB_LEN) {
        memset(output, 0, count*sizeof(*output));
    } else if (fixed >= 0) {
        for (int i=0; i<count; ++i, phase+=step)
            output[i] = sf2_wave(fixed, (phase >> FREQ_SH) + (input ? input[i] >> 1 : 0));
    } else {
        for (int i=0; i<count; ++i, phase+=step)
            output[i] = sf2_wave(level[i], (phase >> FREQ_SH) + (input ? input[i] >> 1 : 0));
    }
    op->phase += step * (uint32_t)count;
}

static void sf2_feedback(YM2151Operator *op, int count, unsigned before,
                         const unsigned *ticks, int *output)
{
    unsigned level[FM_BLOCK];
    int fixed = sf2_levels(op, before, ticks, count, level);
    int prior = op->fb_out_prev, current = op->fb_out_curr;
    uint32_t phase = op->phase, step = op->freq;
    unsigned shift = op->fb_shift;
    for (int i=0; i<count; ++i, phase+=step) {
        unsigned feedback = shift ? (uint32_t)(prior + current) << shift : 0;
        prior = output[i] = current;
        current = sf2_wave(fixed < 0 ? level[i] : (unsigned)fixed,
                           ((phase & ~FREQ_MASK) + feedback) >> FREQ_SH);
    }
    op->fb_out_prev = prior;
    op->fb_out_curr = current;
    op->phase = phase;
}

static __attribute__((noinline)) int sf2_music_block(SAMP *left, SAMP *right, int length)
{
    if (!sf2_music_enabled || PSG->csm_req || (PSG->irq_enable & 128) || (PSG->noise & 128))
        return 0;
    for (unsigned ch=0; ch<8; ++ch) {
        YM2151Operator *op = &PSG->oper[4*ch];
        if ((op->ams && (PSG->amd || PSG->lfa)) ||
            (op->pms && PSG->pmd)) return 0;
    }
    while (length > 0) {
        const int n = length < FM_BLOCK ? length : FM_BLOCK;
        int mix[2][FM_BLOCK] = {{0}};
        unsigned ticks[FM_BLOCK], before = PSG->eg_cnt;
        for (int i=0; i<n; ++i) {
            PSG->eg_timer += PSG->eg_timer_add;
            while (PSG->eg_timer >= PSG->eg_timer_overflow) {
                PSG->eg_timer -= PSG->eg_timer_overflow;
                ++PSG->eg_cnt;
            }
            ticks[i] = PSG->eg_cnt;
            step_timer_a();
            advance_clock();
        }
        for (unsigned ch=0; ch<8; ++ch) {
            YM2151Operator *op = &PSG->oper[4*ch];
            const unsigned alg = PSG->connect[ch];
            if (!(op[0].state | op[1].state | op[2].state | op[3].state) &&
                !(op->mem_value | op->fb_out_prev | op->fb_out_curr)) {
                for (int j = 0; j < 4; ++j) op[j].phase += op[j].freq * (uint32_t)n;
                continue;
            }
            int delayed[FM_BLOCK+1], m2[FM_BLOCK], c1[FM_BLOCK], c2[FM_BLOCK];
            int *m1 = delayed+1;
            delayed[0] = op->mem_value;
            sf2_feedback(op, n, before, ticks, m1);
            if (alg < 4) {
                // MEM is a one-sample delay on the C1 path. Place its
                // previous output before the block, retaining the final
                // value for the next block even across a register write.
                int memory[FM_BLOCK+1];
                memory[0] = op->mem_value;
                sf2_voice(op+2, n, before, ticks, alg == 0 || alg == 3 ? m1 : NULL, memory+1);
                if (alg == 1) {
                    for (int i=0; i<n; ++i) memory[i+1] += m1[i];
                }
                sf2_voice(op+1, n, before, ticks, alg == 3 ? NULL : memory, m2);
                if (alg == 2 || alg == 3) {
                    const int *extra = alg == 2 ? m1 : memory;
                    for (int i=0; i<n; ++i) m2[i] += extra[i];
                }
                sf2_voice(op+3, n, before, ticks, m2, c2);
                op->mem_value = memory[n];
                for (int i=0; i<n; ++i) {
                    mix[0][i] += c2[i] & PSG->pan[ch*2];
                    mix[1][i] += c2[i] & PSG->pan[ch*2+1];
                }
                continue;
            }
            sf2_voice(op+1, n, before, ticks, alg == 5 ? delayed : NULL, m2);
            sf2_voice(op+2, n, before, ticks, alg == 7 ? NULL : m1, c1);
            sf2_voice(op+3, n, before, ticks, alg == 4 ? m2 : alg == 5 ? m1 : NULL, c2);
            if (alg == 5) op->mem_value = m1[n-1];
            for (int i=0; i<n; ++i) {
                int sample = c1[i] + c2[i] + (alg == 4 ? 0 : m2[i]) + (alg == 7 ? m1[i] : 0);
                mix[0][i] += sample & PSG->pan[ch*2];
                mix[1][i] += sample & PSG->pan[ch*2+1];
            }
        }
        for (int i=0; i<n; ++i) {
            int l = mix[0][i] >> FINAL_SH, r = mix[1][i] >> FINAL_SH;
            left[i] = l > MAXOUT ? MAXOUT : l < MINOUT ? MINOUT : l;
            right[i] = r > MAXOUT ? MAXOUT : r < MINOUT ? MINOUT : r;
        }
        left += n; right += n; length -= n;
    }
    return 1;
}
