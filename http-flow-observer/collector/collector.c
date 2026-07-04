#include <stdio.h>
#include <signal.h>

#include <bpf/libbpf.h>

#include "collector.h"
#include "event.h"
#include "event_queue.h"
#include "connection_table.h"
#include "timeline.h"

static volatile sig_atomic_t running = 1;

static struct event_queue queue;

/* ============================================================
 * Event Name
 * ============================================================
 */

static const char *event_name(__u32 event)
{
    switch (event)
    {
        case EVENT_NET_RX:
            return "NET_RX";

        case EVENT_IRQ_ENTRY:
            return "IRQ_ENTRY";

        case EVENT_SOFTIRQ_ENTRY:
            return "SOFTIRQ_ENTRY";

        case EVENT_IP_RCV:
            return "IP_RCV";

        case EVENT_TCP_V4_RCV:
            return "TCP_V4_RCV";

        case EVENT_TCP_DATA_QUEUE:
            return "TCP_DATA_QUEUE";

        case EVENT_SOCK_DEF_READABLE:
            return "SOCK_DEF_READABLE";

        case EVENT_ACCEPT4_ENTER:
            return "ACCEPT4_ENTER";

        case EVENT_ACCEPT4_EXIT:
            return "ACCEPT4_EXIT";

        case EVENT_RECVFROM_ENTER:
            return "RECVFROM_ENTER";

        case EVENT_RECVFROM_EXIT:
            return "RECVFROM_EXIT";

        case EVENT_SENDTO_ENTER:
            return "SENDTO_ENTER";

        case EVENT_SENDTO_EXIT:
            return "SENDTO_EXIT";

        case EVENT_TCP_SENDMSG:
            return "TCP_SENDMSG";

        case EVENT_TCP_WRITE_XMIT:
            return "TCP_WRITE_XMIT";

        case EVENT_IP_OUTPUT:
            return "IP_OUTPUT";

        case EVENT_NET_DEV_QUEUE:
            return "NET_DEV_QUEUE";

        default:
            return "UNKNOWN";
    }
}

/* ============================================================
 * Ring Buffer Callback
 * ============================================================
 */

int collector_handle_event(void *ctx, void *data, size_t len)
{
    struct event *e = data;

    (void)ctx;
    (void)len;

    /* track connection lifecycle */
    connection_table_update(e);

    /* build per-socket request timeline */
    switch (e->event)
    {
        case EVENT_TCP_V4_RCV:
            /* new inbound segment — start (or restart) socket timeline */
            timeline_start(e->socket_ptr, e);
            break;

        case EVENT_TCP_SENDMSG:
            /* response sent — finalise and print timeline */
            timeline_finish(e->socket_ptr, e);
            break;

        default:
            /* append to active timeline for this socket */
            timeline_add_event(e->socket_ptr, e);
            break;
    }

    event_queue_push(&queue, e);

    printf("%-18llu %-6u %-6u %-4u %-20s %-20s\n",
           (unsigned long long)e->timestamp,
           e->pid,
           e->tid,
           e->cpu,
           e->comm,
           event_name(e->event));

    return 0;
}

/* ============================================================
 * Lost Events
 * ============================================================
 */

void handle_lost_events(void *ctx,
                        int cpu,
                        __u64 lost)
{
    fprintf(stderr,
            "Lost %llu events on CPU %d\n",
            lost,
            cpu);
}

/* ============================================================
 * Signal Handler
 * ============================================================
 */

static void signal_handler(int sig)
{
    running = 0;
}

/* ============================================================
 * Collector Loop
 * ============================================================
 */

int collector_run(struct ring_buffer *rb)
{
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    event_queue_init(&queue);
    connection_table_init();
    timeline_init();

    printf("\n");

    printf("%-18s %-6s %-6s %-4s %-20s %-20s\n",
           "Timestamp(ns)",
           "PID",
           "TID",
           "CPU",
           "Process",
           "Event");

    printf("-------------------------------------------------------------------------------\n");

    while (running)
    {
        ring_buffer__poll(rb, 100);
    }

    return 0;
}
