#include <assert.h>
#include <limits.h>

#include "../log_queue.h"
#include "../perf_stats.h"

static void test_log_overflow_and_wrap(void) {
    LogQueue queue = {.write = UINT_MAX, .read = UINT_MAX};
    char line[LOG_LINE_SIZE];
    assert(log_queue_push(&queue, "first"));
    for (unsigned i = 1; i < LOG_QUEUE_CAPACITY; i++) {
        assert(!log_queue_push(&queue, "next"));
    }
    assert(!log_queue_push(&queue, "discarded"));
    assert(queue.dropped == 1);
    assert(log_queue_pop(&queue, line) && strcmp(line, "first") == 0);
    for (unsigned i = 1; i < LOG_QUEUE_CAPACITY; i++) {
        assert(log_queue_pop(&queue, line) && strcmp(line, "next") == 0);
    }
    assert(!log_queue_pop(&queue, line));
    assert(log_queue_push(&queue, "new"));
    assert(log_queue_pop(&queue, line) && strcmp(line, "new") == 0);
}

static void test_logic_windows(void) {
    LogicWindow report = {0}, trace = {0};
    logic_record(&report, 1000, 10000, 2000, 3000, 40, 1, 20000);
    logic_record(&trace, 1000, 10000, 2000, 3000, 40, 1, 20000);
    LogicWindow first = logic_take(&trace);
    assert(first.samples == 1 && first.peak_us == 20000);
    assert(logic_take(&trace).samples == 0);
    logic_record(&report, 3000, 18000, 0, 5000, 60, 2, 30000);
    logic_record(&trace, 3000, 18000, 0, 5000, 60, 2, 30000);
    LogicWindow combined = logic_take(&report);
    assert(combined.samples == 2 && combined.pads == 3 && combined.batches == 100);
    assert(combined.input_max == 3000 && combined.peak_us == 30000);
    assert(logic_average_ms(combined.input, combined.samples) == 2.0);
    assert(logic_average_ms(combined.logic, combined.samples) == 14.0);
    assert(logic_average_ms(combined.layout, combined.samples) == 1.0);
    assert(logic_average_ms(combined.emit, combined.samples) == 4.0);
    assert(logic_average_ms(0, 0) == 0.0);
    assert(logic_take(&trace).samples == 1);
}

int main(void) {
    test_log_overflow_and_wrap();
    test_logic_windows();
}
