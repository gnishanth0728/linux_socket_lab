#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include <bpf/libbpf.h>

#include "kernel/tracer.skel.h"
#include "collector/event.h"
#include <inttypes.h>
static volatile sig_atomic_t running = 1;

/* ============================================================
 * Event Structure
 * ============================================================
 */



/* ============================================================
 * Event Names
 * ============================================================
 */

static const char *event_name(unsigned int event)
{
    switch (event)
    {
        case EVENT_NET_RX:
            return "NET_RX";

        case EVENT_IRQ_ENTRY:
            return "IRQ_ENTRY";

        case EVENT_SOFTIRQ_ENTRY:
            return "SOFTIRQ";

        case EVENT_IP_RCV:
            return "IP_RCV";

        case EVENT_TCP_V4_RCV:
            return "TCP_V4_RCV";

        case EVENT_TCP_DATA_QUEUE:
            return "TCP_DATA_QUEUE";

        case EVENT_SOCK_DEF_READABLE:
            return "SOCK_READABLE";

        case EVENT_RECVFROM_ENTER:
            return "RECVFROM";

        case EVENT_SENDTO_ENTER:
            return "SENDTO";

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


static int handle_event(void *ctx, void *data, size_t size)
{
    struct event *e = data;

    printf("%-18" PRIu64 " %-6u %-4u %-16s %-20s "
           "%08x:%u -> %08x:%u proto=%u len=%u\n",
           e->timestamp,
           e->pid,
           e->cpu,
           e->comm,
           event_name(e->event),
           e->saddr,
           e->sport,
           e->daddr,
           e->dport,
           e->protocol,
           e->packet_len);

    return 0;
}

/* ============================================================
 * Signal
 * ============================================================
 */

static void sig_handler(int signo)
{
    running = 0;
}

/* ============================================================
 * Main
 * ============================================================
 */

int main(void)
{
    struct tracer_bpf *skel;

    struct ring_buffer *rb;

    signal(SIGINT, sig_handler);

    signal(SIGTERM, sig_handler);

    libbpf_set_strict_mode(LIBBPF_STRICT_ALL);

    skel = tracer_bpf__open();

    if (!skel)
    {
        fprintf(stderr, "Failed to open skeleton\n");
        return 1;
    }

    if (tracer_bpf__load(skel))
    {
        fprintf(stderr, "Failed to load BPF\n");
        return 1;
    }

    if (tracer_bpf__attach(skel))
    {
        fprintf(stderr, "Failed to attach\n");
        return 1;
    }

    rb = ring_buffer__new(
            bpf_map__fd(skel->maps.events),
            handle_event,
            NULL,
            NULL);

    if (!rb)
    {
        fprintf(stderr, "Failed to create ring buffer\n");
        return 1;
    }

    printf("\n");
    printf("===============================================\n");
    printf("HTTP FLOW OBSERVER\n");
    printf("===============================================\n\n");

    printf("%-18s %-6s %-4s %-16s %-20s %-24s %-8s %-6s\n",
       "Timestamp",
       "PID",
       "CPU",
       "Process",
       "Event",
       "Source -> Destination",
       "Proto",
       "Len");

    while (running)
    {
        ring_buffer__poll(rb, 100);
    }

    ring_buffer__free(rb);

    tracer_bpf__destroy(skel);

    return 0;
}