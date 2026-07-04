#include "connection_table.h"

#include <string.h>

/* ============================================================
 * Hash Table
 * ============================================================
 */

#define CONN_TABLE_SIZE 1024

static struct connection_entry table[CONN_TABLE_SIZE];
static int                     occupied[CONN_TABLE_SIZE];

/* ============================================================
 * Hash
 * ============================================================
 */

static unsigned int hash_socket(uint64_t socket)
{
    return (unsigned int)((socket ^ (socket >> 32)) % CONN_TABLE_SIZE);
}

/* ============================================================
 * Init
 * ============================================================
 */

void connection_table_init(void)
{
    memset(table,    0, sizeof(table));
    memset(occupied, 0, sizeof(occupied));
}

/* ============================================================
 * Find
 * ============================================================
 */

struct connection_entry *connection_table_find(uint64_t socket)
{
    unsigned int idx   = hash_socket(socket);
    unsigned int start = idx;

    do
    {
        if (occupied[idx] && table[idx].socket == socket)
            return &table[idx];

        idx = (idx + 1) % CONN_TABLE_SIZE;
    }
    while (idx != start);

    return NULL;
}

/* ============================================================
 * Allocate Slot
 * ============================================================
 */

static struct connection_entry *alloc_slot(uint64_t socket)
{
    unsigned int idx   = hash_socket(socket);
    unsigned int start = idx;

    do
    {
        if (!occupied[idx])
        {
            occupied[idx] = 1;
            memset(&table[idx], 0, sizeof(table[idx]));
            return &table[idx];
        }

        idx = (idx + 1) % CONN_TABLE_SIZE;
    }
    while (idx != start);

    return NULL;
}

/* ============================================================
 * Update
 * ============================================================
 */

void connection_table_update(const struct event *e)
{
    uint64_t                 socket = e->socket_ptr;
    struct connection_entry *entry;

    if (!socket)
        return;

    entry = connection_table_find(socket);

    if (!entry)
    {
        entry = alloc_slot(socket);

        if (!entry)
            return;

        entry->socket     = socket;
        entry->state      = CONN_STATE_NEW;
        entry->first_seen = e->timestamp;
        entry->pid        = e->pid;
        entry->saddr      = e->saddr;
        entry->daddr      = e->daddr;
        entry->sport      = e->sport;
        entry->dport      = e->dport;
    }

    entry->last_event  = *e;
    entry->event_count++;

    /* update address info when available */
    if (e->saddr)
    {
        entry->saddr = e->saddr;
        entry->daddr = e->daddr;
        entry->sport = e->sport;
        entry->dport = e->dport;
    }

    /* advance socket lifecycle state */
    switch (e->event)
    {
        case EVENT_ACCEPT4_EXIT:
            entry->state = CONN_STATE_ACCEPT;
            break;

        case EVENT_TCP_DATA_QUEUE:
        case EVENT_SOCK_DEF_READABLE:
        case EVENT_RECVFROM_ENTER:
        case EVENT_SENDTO_ENTER:
        case EVENT_TCP_SENDMSG:
            if (entry->state == CONN_STATE_NEW ||
                entry->state == CONN_STATE_ACCEPT)
            {
                entry->state = CONN_STATE_ACTIVE;
            }
            break;

        default:
            break;
    }
}
