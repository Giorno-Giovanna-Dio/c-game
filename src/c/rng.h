#ifndef RNG_H
#define RNG_H

#include <stdint.h>

typedef struct {
    uint64_t s[2];
} rc_rng_t;

void rc_rng_seed(rc_rng_t *r, uint32_t seed);
uint32_t rc_rng_u32(rc_rng_t *r);
int rc_rng_range(rc_rng_t *r, int lo_inclusive, int hi_inclusive);

#endif
