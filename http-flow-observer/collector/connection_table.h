#ifndef CONNECTION_TABLE_H
#define CONNECTION_TABLE_H

#include "event.h"

struct connection_entry
{
    uint64_t socket;

    struct event last_event;

    unsigned long event_count;
};

void connection_table_init(void);

void connection_table_update(const struct event *e);

struct connection_entry *
connection_table_find(uint64_t socket);

#endif