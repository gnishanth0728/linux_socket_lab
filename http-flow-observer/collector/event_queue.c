#include "event_queue.h"

#include <string.h>

void event_queue_init(struct event_queue *queue)
{
    memset(queue, 0, sizeof(*queue));
}

bool event_queue_is_empty(const struct event_queue *queue)
{
    return queue->count == 0;
}

bool event_queue_is_full(const struct event_queue *queue)
{
    return queue->count == EVENT_QUEUE_SIZE;
}

bool event_queue_push(struct event_queue *queue,
                      const struct event *event)
{
    if (event_queue_is_full(queue))
        return false;

    queue->buffer[queue->tail] = *event;

    queue->tail++;

    if (queue->tail >= EVENT_QUEUE_SIZE)
        queue->tail = 0;

    queue->count++;

    return true;
}

bool event_queue_pop(struct event_queue *queue,
                     struct event *event)
{
    if (event_queue_is_empty(queue))
        return false;

    *event = queue->buffer[queue->head];

    queue->head++;

    if (queue->head >= EVENT_QUEUE_SIZE)
        queue->head = 0;

    queue->count--;

    return true;
}

unsigned int event_queue_size(const struct event_queue *queue)
{
    return queue->count;
}

void event_queue_clear(struct event_queue *queue)
{
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;

    memset(queue->buffer, 0, sizeof(queue->buffer));
}