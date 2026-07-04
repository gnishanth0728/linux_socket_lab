#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include <bpf/libbpf.h>

#include "kernel/tracer.skel.h"

static volatile sig_atomic_t running = 1;

/* ============================================================
 * Event Structure
 * ============================================================
 */

struct event
{
    unsigned long long timestamp;

    unsigned int pid;
    unsigned int tid;

    unsigned int uid;
    unsigned int gid;

    unsigned int cpu;

    unsigned int event;

    char comm[16];
};

/* ============================================================
 * Event Names
 * ============================================================
 */

static const char *event_name(unsigned int event)
{
    switch (event)
    {
        case 5:
            return "TCP_V4_RCV";

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

    printf("%-18llu %-6u %-4u %-16s %-20s\n",
           e->timestamp,
           e->pid,
           e->cpu,
           e->comm,
           event_name(e->event));

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

    printf("%-18llu %-6u %-4u %-16s %-20s "
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

    while (running)
    {
        ring_buffer__poll(rb, 100);
    }

    ring_buffer__free(rb);

    tracer_bpf__destroy(skel);

    return 0;
}