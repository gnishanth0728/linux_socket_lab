// SPDX-License-Identifier: GPL-2.0

/*
 * ============================================================================
 * File        : tcp_monitor.bpf.c
 * Description : eBPF program that traces outgoing TCP connections.
 *
 * Hook:
 *      tcp_v4_connect()
 *
 * Captures:
 *      - Timestamp
 *      - PID
 *      - Process Name
 *      - Source IP
 *      - Destination IP
 *      - Source Port
 *      - Destination Port
 *
 * Event Transport:
 *      Ring Buffer
 * ============================================================================
 */
#define __BPF__
#include "vmlinux.h"
#include "common.h"

#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_endian.h>




/* ============================================================================
 * Ring Buffer Map
 * ============================================================================
 */

struct {

    __uint(type, BPF_MAP_TYPE_RINGBUF);

    __uint(max_entries, 1 << 24);

} events SEC(".maps");

/* ============================================================================
 * Helper
 *
 * Reserve an event from the ring buffer.
 * ============================================================================
 */

static __always_inline struct tcp_event *
reserve_event(void)
{
    return bpf_ringbuf_reserve(
            &events,
            sizeof(struct tcp_event),
            0);
}

/* ============================================================================
 * Helper
 *
 * Fill common process information.
 * ============================================================================
 */

static __always_inline void
fill_process_info(struct tcp_event *event)
{
    event->timestamp_ns = bpf_ktime_get_ns();

    event->pid =
        bpf_get_current_pid_tgid() >> 32;

    bpf_get_current_comm(
        event->comm,
        sizeof(event->comm));
}

/* ============================================================================
 * Helper
 *
 * Read IPv4 information from struct sock.
 *
 * struct sock
 *
 *     sk->__sk_common.skc_rcv_saddr
 *     sk->__sk_common.skc_daddr
 *     sk->__sk_common.skc_num
 *     sk->__sk_common.skc_dport
 *
 * ============================================================================
 */

static __always_inline void
fill_socket_info(
        struct tcp_event *event,
        struct sock *sk)
{
    if (!sk)
        return;

    event->saddr =
        BPF_CORE_READ(
            sk,
            __sk_common.skc_rcv_saddr);

    event->daddr =
        BPF_CORE_READ(
            sk,
            __sk_common.skc_daddr);

    event->sport =
        BPF_CORE_READ(
            sk,
            __sk_common.skc_num);

    event->dport =
        bpf_ntohs(
            BPF_CORE_READ(
                sk,
                __sk_common.skc_dport));
}

/* ============================================================================
 * Helper
 *
 * Submit event to user space.
 * ============================================================================
 */

static __always_inline void
submit_event(struct tcp_event *event)
{
    if (!event)
        return;

    bpf_ringbuf_submit(event, 0);
}

/* ============================================================================
 * Kernel Hook
 *
 * This function executes every time the Linux kernel calls:
 *
 *      tcp_v4_connect()
 *
 * This happens when a process executes:
 *
 *      connect()
 *
 * Examples:
 *
 *      curl
 *      nginx
 *      java
 *      postgres
 *      psql
 *
 * ============================================================================
 */

SEC("kprobe/tcp_v4_connect")
int BPF_KPROBE(
        trace_tcp_connect,
        struct sock *sk)
{
    struct tcp_event *event;

    event = reserve_event();

    if (!event)
        return 0;

    fill_process_info(event);

    fill_socket_info(event, sk);

       /*
     * Send the completed event to user space.
     */
    submit_event(event);

    return 0;
}

/* ============================================================================
 * Future Hooks
 *
 * We will add these in later parts.
 *
 * tcp_v6_connect()      -> IPv6 outgoing connections
 * inet_csk_accept()     -> Server-side accept()
 * tcp_close()           -> Connection close
 * tcp_sendmsg()         -> Data sent
 * tcp_recvmsg()         -> Data received
 * ============================================================================
 */

/*
SEC("kprobe/tcp_v6_connect")
int BPF_KPROBE(trace_tcp_v6_connect, struct sock *sk)
{
    return 0;
}

SEC("kprobe/inet_csk_accept")
int BPF_KPROBE(trace_accept)
{
    return 0;
}

SEC("kprobe/tcp_close")
int BPF_KPROBE(trace_close)
{
    return 0;
}

SEC("kprobe/tcp_sendmsg")
int BPF_KPROBE(trace_send)
{
    return 0;
}

SEC("kprobe/tcp_recvmsg")
int BPF_KPROBE(trace_recv)
{
    return 0;
}
*/

/* ============================================================================
 * License
 *
 * Required by the kernel.
 * ============================================================================
 */

char LICENSE[] SEC("license") = "GPL";