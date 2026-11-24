#ifndef BITCRUSHER_H
#define BITCRUSHER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int16_t held_sample;
    uint8_t frames_until_refresh;
    uint8_t previous_bit_depth;
    uint8_t previous_hold_factor;
    uint8_t initialized;
} BitcrusherState;

void bitcrusher_init(BitcrusherState *state);

/*
 * Quantize one signed 16-bit PCM sample and optionally hold it for several
 * output frames. bit_depth is clamped to 1..16; hold_factor to 1..8.
 */
int16_t bitcrusher_process_sample(
    BitcrusherState *state,
    int16_t input_sample,
    uint8_t bit_depth,
    uint8_t hold_factor
);

#ifdef __cplusplus
}
#endif

#endif
