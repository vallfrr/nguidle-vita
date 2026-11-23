#ifndef NGU_LOG_QUEUE_H
#define NGU_LOG_QUEUE_H

#include <string.h>

enum { LOG_QUEUE_CAPACITY = 16, LOG_LINE_SIZE = 1536 };

typedef struct {
    char lines[LOG_QUEUE_CAPACITY][LOG_LINE_SIZE];
    unsigned write, read, dropped;
} LogQueue;

// Access is serialized by the caller. Signal the worker on an empty-to-ready transition.
static inline int log_queue_push(LogQueue *queue, const char *line) {
    unsigned count = queue->write - queue->read;
    if (count == LOG_QUEUE_CAPACITY) {
        queue->dropped++;
        return 0;
    }
    strcpy(queue->lines[queue->write % LOG_QUEUE_CAPACITY], line);
    queue->write++;
    return count == 0;
}

static inline int log_queue_pop(LogQueue *queue, char *line) {
    if (queue->read == queue->write) return 0;
    strcpy(line, queue->lines[queue->read % LOG_QUEUE_CAPACITY]);
    queue->read++;
    return 1;
}

#endif
