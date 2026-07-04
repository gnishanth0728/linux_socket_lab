#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include <bpf/libbpf.h>

#include "collector/event.h"
#include "collector/event_queue.h"

/* Generated Skeleton Headers */
#include "kernel/nic.skel.h"
#include "kernel/irq.skel.h"
#include "kernel/ip.skel.h"
#include "kernel/tcp.skel.h"
#include "kernel/socket.skel.h"
#include "kernel/send.skel.h"

/* Collector */
extern int collector_run(struct ring_buffer *rb);

static struct ring_buffer *rb = NULL;

int main(void)
{
    struct nic_bpf *nic = NULL;
    struct irq_bpf *irq = NULL;
    struct ip_bpf *ip = NULL;
    struct tcp_bpf *tcp = NULL;
    struct socket_bpf *socket = NULL;
    struct send_bpf *send = NULL;

    libbpf_set_strict_mode(LIBBPF_STRICT_ALL);

    /* ==========================================================
     * Open
     * ==========================================================
     */

    nic = nic_bpf__open();
    irq = irq_bpf__open();
    ip = ip_bpf__open();
    tcp = tcp_bpf__open();
    socket = socket_bpf__open();
    send = send_bpf__open();

    if (!nic || !irq || !ip || !tcp || !socket || !send)
    {
        fprintf(stderr, "Failed to open BPF skeletons\n");
        return EXIT_FAILURE;
    }

    /* ==========================================================
     * Load
     * ==========================================================
     */

    if (nic_bpf__load(nic))
        return EXIT_FAILURE;

    if (irq_bpf__load(irq))
        return EXIT_FAILURE;

    if (ip_bpf__load(ip))
        return EXIT_FAILURE;

    if (tcp_bpf__load(tcp))
        return EXIT_FAILURE;

    if (socket_bpf__load(socket))
        return EXIT_FAILURE;

    if (send_bpf__load(send))
        return EXIT_FAILURE;

    /* ==========================================================
     * Attach
     * ==========================================================
     */

    nic_bpf__attach(nic);

    irq_bpf__attach(irq);

    ip_bpf__attach(ip);

    tcp_bpf__attach(tcp);

    socket_bpf__attach(socket);

    send_bpf__attach(send);

    /* ==========================================================
     * Ring Buffer
     * ==========================================================
     */

    rb = ring_buffer__new(
            bpf_map__fd(nic->maps.events),
            handle_event,
            NULL,
            NULL);

    if (!rb)
    {
        fprintf(stderr, "Failed to create ring buffer\n");
        return EXIT_FAILURE;
    }

    printf("\n");
    printf("===============================================\n");
    printf(" HTTP FLOW OBSERVER STARTED\n");
    printf("===============================================\n\n");

    collector_run(rb);

    /* ==========================================================
     * Cleanup
     * ==========================================================
     */

    ring_buffer__free(rb);

    send_bpf__destroy(send);
    socket_bpf__destroy(socket);
    tcp_bpf__destroy(tcp);
    ip_bpf__destroy(ip);
    irq_bpf__destroy(irq);
    nic_bpf__destroy(nic);

    return EXIT_SUCCESS;
}