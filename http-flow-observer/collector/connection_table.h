#ifndef CONNECTION_TABLE_H
#define CONNECTION_TABLE_H

#include "event.h"

enum conn_state
{
    CONN_STATE_NEW = 0,
    CONN_STATE_ACCEPT,
    CONN_STATE_ACTIVE,
    CONN_STATE_CLOSED
};

struct connection_entry
{
    uint64_t        socket;

    enum conn_state state;

    uint64_t        first_seen;

    uint32_t        pid;

    uint32_t        saddr;
    uint32_t        daddr;
    uint16_t        sport;
    uint16_t        dport;

    struct event    last_event;

    unsigned long   event_count;
};

void connection_table_init(void);

void connection_table_update(const struct event *e);

struct connection_entry *
connection_table_find(uint64_t socket);

#endif
