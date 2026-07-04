#ifndef HTTP_FLOW_TIMELINE_H
#define HTTP_FLOW_TIMELINE_H

#include "event.h"

#include <stdbool.h>
#include <stdint.h>

/* ============================================================
 * Timeline Entry
 * ============================================================
 */

#define TIMELINE_MAX_EVENTS 64

struct timeline_entry
{
    struct event    e;
    uint64_t        delta_ns;   /* ns since previous event in this timeline */
};

/* ============================================================
 * Per-Socket Timeline
 * ============================================================
 */

struct timeline
{
    uint64_t              socket;

    struct timeline_entry entries[TIMELINE_MAX_EVENTS];

    unsigned int          count;

    uint64_t              start_ts;   /* timestamp of first event */

    bool                  active;     /* currently recording a request */

    bool                  occupied;   /* slot in use */
};

/* ============================================================
 * API
 * ============================================================
 */

void timeline_init(void);

/*
 * Called on TCP_V4_RCV: open a new timeline for this socket.
 * Any previous timeline for the socket is discarded.
 */
void timeline_start(uint64_t socket, const struct event *e);

/*
 * Append an event to an active timeline.  No-op if no active
 * timeline exists for this socket.
 */
void timeline_add_event(uint64_t socket, const struct event *e);

/*
 * Called on TCP_SENDMSG: append final event, print, and close
 * the timeline for this socket.
 */
void timeline_finish(uint64_t socket, const struct event *e);

void timeline_print(uint64_t socket);

void timeline_clear(uint64_t socket);

#endif
