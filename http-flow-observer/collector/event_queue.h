#ifndef HTTP_FLOW_EVENT_QUEUE_H
#define HTTP_FLOW_EVENT_QUEUE_H

#include <stdbool.h>

#include "event.h"

#define EVENT_QUEUE_SIZE 4096

struct event_queue
{
    struct event buffer[EVENT_QUEUE_SIZE];

    unsigned int head;

    unsigned int tail;

    unsigned int count;
};

void event_queue_init(struct event_queue *queue);

bool event_queue_is_empty(const struct event_queue *queue);

bool event_queue_is_full(const struct event_queue *queue);

bool event_queue_push(struct event_queue *queue,
                      const struct event *event);

bool event_queue_pop(struct event_queue *queue,
                     struct event *event);

unsigned int event_queue_size(const struct event_queue *queue);

void event_queue_clear(struct event_queue *queue);

#endif