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
static void ipv4_to_string(uint32_t addr, char *buf)
{
    sprintf(buf,
            "%u.%u.%u.%u",
            addr & 0xff,
            (addr >> 8) & 0xff,
            (addr >> 16) & 0xff,
            (addr >> 24) & 0xff);
}

static int handle_event(void *ctx, void *data, size_t size)
{
    struct event *e = data;

    char src[32];
    char dst[32];

    ipv4_to_string(e->saddr, src);
    ipv4_to_string(e->daddr, dst);

    printf("%-18llu %-6u %-4u %-16s %-20s\n",
           (unsigned long long)e->timestamp,
           e->pid,
           e->cpu,
           e->comm,
           event_name(e->event));

    printf("    %s:%u -> %s:%u  proto=%u len=%u\n\n",
           src,
           e->sport,
           dst,
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