#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

#include <bpf/libbpf.h>

#include "collector.h"
#include "event.h"
#include "event_queue.h"
#include "connection_table.h"
#include "timeline.h"

static volatile sig_atomic_t running = 1;

static struct event_queue queue;

static int verbose_events = 0;
static uint16_t filter_port = 0;   /* 0 = all ports */

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

        case EVENT_NAPI_POLL:
            return "NAPI_POLL";

        case EVENT_ETHERNET_RX:
            return "ETHERNET_RX";

        case EVENT_IRQ_ENTRY:
            return "IRQ_ENTRY";

        case EVENT_SOFTIRQ_ENTRY:
            return "SOFTIRQ_ENTRY";

        case EVENT_IP_RCV:
            return "IP_RCV";

        case EVENT_NETFILTER_HOOK:
            return "NETFILTER_HOOK";

        case EVENT_ROUTE_LOOKUP:
            return "ROUTE_LOOKUP";

        case EVENT_TCP_V4_RCV:
            return "TCP_V4_RCV";

        case EVENT_TCP_STATE_MACHINE:
            return "TCP_STATE_MACHINE";

        case EVENT_TCP_DATA_QUEUE:
            return "TCP_DATA_QUEUE";

        case EVENT_SOCK_DEF_READABLE:
            return "SOCK_DEF_READABLE";

        case EVENT_SCHED_WAKEUP:
            return "SCHED_WAKEUP";

        case EVENT_SCHED_SWITCH:
            return "SCHED_SWITCH";

        case EVENT_NGINX_HTTP_PARSE:
            return "NGINX_HTTP_PARSE";

        case EVENT_NGINX_REVERSE_PROXY:
            return "NGINX_REVERSE_PROXY";

        case EVENT_NGINX_BACKEND_SOCKET:
            return "NGINX_BACKEND_SOCKET";

        case EVENT_NGINX_RESPONSE_GEN:
            return "NGINX_RESPONSE_GEN";

        case EVENT_NGINX_RESPONSE_TX:
            return "NGINX_RESPONSE_TX";

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
            /* Only track this connection if it matches the port filter.
             * dport is the server-side listening port (e.g. 8080). */
            if (filter_port == 0
                || e->dport == filter_port
                || e->sport == filter_port)
            {
                timeline_start(e->socket_ptr, e);
            }
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

    if (verbose_events)
    {
        printf("%-18llu %-6u %-6u %-4u %-20s %-20s\n",
               (unsigned long long)e->timestamp,
               e->pid,
               e->tid,
               e->cpu,
               e->comm,
               event_name(e->event));
    }

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
    const char *v;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    v = getenv("HTTP_FLOW_VERBOSE_EVENTS");
    verbose_events = (v && *v && v[0] != '0') ? 1 : 0;

    {
        const char *p = getenv("HTTP_FLOW_PORT");
        if (p && *p)
        {
            int port_val = atoi(p);
            if (port_val > 0 && port_val <= 65535)
                filter_port = (uint16_t)port_val;
        }
    }

    event_queue_init(&queue);
    connection_table_init();
    timeline_init();

    printf("\n");
    printf("HTTP Flow Observer\n");
    printf("==================\n");

    if (filter_port > 0)
        printf("Tracking port : %u only\n", filter_port);
    else
        printf("Tracking port : all  (set HTTP_FLOW_PORT=8080 to filter)\n");

    if (verbose_events)
        printf("Event stream  : verbose\n");
    else
        printf("Event stream  : request-centric  (set HTTP_FLOW_VERBOSE_EVENTS=1 for raw stream)\n");

    printf("\n");

    if (verbose_events)
    {
        printf("%-18s %-6s %-6s %-4s %-20s %-20s\n",
               "Timestamp(ns)",
               "PID",
               "TID",
               "CPU",
               "Process",
               "Event");

        printf("-------------------------------------------------------------------------------\n");
    }

    while (running)
    {
        ring_buffer__poll(rb, 100);
    }

    return 0;
}
