#include "bitcrusher.h"

#include <math.h>
#include <stddef.h>

static uint8_t clamp_bit_depth(uint8_t bit_depth)
{
    if (bit_depth < 1U) {
        return 1U;
    }
    if (bit_depth > 16U) {
        return 16U;
    }
    return bit_depth;
}

static uint8_t clamp_hold_factor(uint8_t hold_factor)
{
    if (hold_factor < 1U) {
        return 1U;
    }
    if (hold_factor > 8U) {
        return 8U;
    }
    return hold_factor;
}

static int16_t quantize_sample(int16_t sample, uint8_t bit_depth)
{
    const int32_t step = (int32_t)1 << (16U - bit_depth);
    const int32_t largest_positive_step = (INT16_MAX / step) * step;
    int32_t quantized = (int32_t)lroundf((float)sample / (float)step) * step;

    if (quantized > largest_positive_step) {
        quantized = largest_positive_step;
    } else if (quantized < INT16_MIN) {
        quantized = INT16_MIN;
    }

    return (int16_t)quantized;
}

void bitcrusher_init(BitcrusherState *state)
{
    if (state == NULL) {
        return;
    }

    state->held_sample = 0;
    state->frames_until_refresh = 0U;
    state->previous_bit_depth = 0U;
    state->previous_hold_factor = 0U;
    state->initialized = 0U;
}

int16_t bitcrusher_process_sample(
    BitcrusherState *state,
    int16_t input_sample,
    uint8_t bit_depth,
    uint8_t hold_factor
)
{
    if (state == NULL) {
        return input_sample;
    }

    bit_depth = clamp_bit_depth(bit_depth);
    hold_factor = clamp_hold_factor(hold_factor);

    if (state->initialized == 0U ||
        state->frames_until_refresh == 0U ||
        state->previous_bit_depth != bit_depth ||
        state->previous_hold_factor != hold_factor) {
        state->held_sample = quantize_sample(input_sample, bit_depth);
        state->frames_until_refresh = hold_factor;
        state->previous_bit_depth = bit_depth;
        state->previous_hold_factor = hold_factor;
        state->initialized = 1U;
    }

    --state->frames_until_refresh;
    return state->held_sample;
}
