#include "rng.h"

/* SplitMix64 種子 xoshiro128** 的簡化版：教學用可重現 RNG */
void rc_rng_seed(rc_rng_t *r, uint32_t seed) {
    uint64_t z = (uint64_t)seed + 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    r->s[0] = z ^ (z >> 31);
    z = r->s[0] + 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    r->s[1] = z ^ (z >> 31);
    if (r->s[0] == 0 && r->s[1] == 0) {
        r->s[0] = 1;
    }
}

static uint64_t rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }

uint32_t rc_rng_u32(rc_rng_t *r) {
    uint64_t s0 = r->s[0];
    uint64_t s1 = r->s[1];
    uint64_t rslt = rotl(s0 * 5, 7) * 9;
    s1 ^= s0;
    r->s[0] = rotl(s0, 24) ^ s1 ^ (s1 << 16);
    r->s[1] = rotl(s1, 37);
    return (uint32_t)(rslt >> 32);
}

int rc_rng_range(rc_rng_t *r, int lo, int hi) {
    if (lo > hi) {
        int t = lo;
        lo = hi;
        hi = t;
    }
    uint32_t span = (uint32_t)(hi - lo + 1);
    return lo + (int)(rc_rng_u32(r) % span);
}
