#ifndef NGU_PERF_STATS_H
#define NGU_PERF_STATS_H

#include <stdint.h>

typedef struct {
    uint32_t info[8];
    uint64_t game_us, debt_us;
} LogicSnapshot;

typedef struct {
    uint64_t input, logic, layout, emit, batches;
    unsigned samples, input_max, pads, peak_us;
} LogicWindow;

static inline void logic_record(LogicWindow *window, unsigned input, unsigned logic,
                                unsigned layout, unsigned emit, unsigned batches,
                                unsigned pads, unsigned iteration_us) {
    window->input += input;
    window->logic += logic;
    window->layout += layout;
    window->emit += emit;
    window->batches += batches;
    window->pads += pads;
    window->samples++;
    if (input > window->input_max) window->input_max = input;
    if (iteration_us > window->peak_us) window->peak_us = iteration_us;
}

static inline LogicWindow logic_take(LogicWindow *window) {
    LogicWindow result = *window;
    *window = (LogicWindow){0};
    return result;
}

static inline double logic_average_ms(uint64_t total_us, unsigned samples) {
    return samples ? total_us / (1000.0 * samples) : 0;
}

#endif
