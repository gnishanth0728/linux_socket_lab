// SPDX-License-Identifier: GPL-2.0

#include "../vmlinux.h"

#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#include "event.bpf.h"

char LICENSE[] SEC("license") = "GPL";

/* ============================================================
 * Ring Buffer
 * ============================================================
 */

struct
{
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 24);
} events SEC(".maps");

/* ============================================================
 * Helper
 * ============================================================
 */

static __always_inline int submit_event(__u32 type)
{
    struct event *e;

    __u64 pid_tgid;

    e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);

    if (!e)
        return 0;

    pid_tgid = bpf_get_current_pid_tgid();

    e->timestamp = bpf_ktime_get_ns();

    e->pid = pid_tgid >> 32;
    e->tid = (__u32)pid_tgid;

    e->uid = 0;
    e->gid = 0;

    e->cpu = bpf_get_smp_processor_id();

    e->event = type;

    bpf_get_current_comm(e->comm, sizeof(e->comm));

    bpf_ringbuf_submit(e, 0);

    return 0;
}

/* ============================================================
 * Test Probe
 * ============================================================
 */

SEC("kprobe/tcp_v4_rcv")
int BPF_KPROBE(trace_tcp_v4_rcv)
{
    return submit_event(EVENT_TCP_V4_RCV);
}

SEC("tracepoint/syscalls/sys_enter_recvfrom")
int trace_recvfrom_enter(struct trace_event_raw_sys_enter *ctx)
{
    return submit_event(EVENT_RECVFROM_ENTER);
}

/* ============================================================
 * NIC Receive
 * ============================================================
 */

SEC("tracepoint/net/netif_receive_skb")
int trace_netif_receive_skb(struct trace_event_raw_net_dev_template *ctx)
{
    return submit_event(EVENT_NET_RX);
}

/* ============================================================
 * IRQ
 * ============================================================
 */

SEC("tracepoint/irq/irq_handler_entry")
int trace_irq_handler_entry(struct trace_event_raw_irq_handler_entry *ctx)
{
    return submit_event(EVENT_IRQ_ENTRY);
}

/* ============================================================
 * SoftIRQ
 * ============================================================
 */

SEC("tracepoint/irq/softirq_entry")
int trace_softirq_entry(struct trace_event_raw_softirq *ctx)
{
    if (ctx->vec != 3)
        return 0;

    return submit_event(EVENT_SOFTIRQ_ENTRY);
}

/* ============================================================
 * IP Receive
 * ============================================================
 */

SEC("kprobe/ip_rcv")
int BPF_KPROBE(trace_ip_rcv)
{
    return submit_event(EVENT_IP_RCV);
}

/* ============================================================
 * TCP Receive
 * ============================================================
 */

SEC("kprobe/tcp_v4_rcv")
int BPF_KPROBE(trace_tcp_v4_rcv)
{
    return submit_event(EVENT_TCP_V4_RCV);
}

/* ============================================================
 * TCP Queue
 * ============================================================
 */

SEC("kprobe/tcp_data_queue")
int BPF_KPROBE(trace_tcp_data_queue)
{
    return submit_event(EVENT_TCP_DATA_QUEUE);
}

/* ============================================================
 * Socket Readable
 * ============================================================
 */

SEC("kprobe/sock_def_readable")
int BPF_KPROBE(trace_sock_def_readable)
{
    return submit_event(EVENT_SOCK_DEF_READABLE);
}

/* ============================================================
 * recvfrom()
 * ============================================================
 */

SEC("tracepoint/syscalls/sys_enter_recvfrom")
int trace_recvfrom_enter(struct trace_event_raw_sys_enter *ctx)
{
    return submit_event(EVENT_RECVFROM_ENTER);
}

/* ============================================================
 * sendto()
 * ============================================================
 */

SEC("tracepoint/syscalls/sys_enter_sendto")
int trace_sendto_enter(struct trace_event_raw_sys_enter *ctx)
{
    return submit_event(EVENT_SENDTO_ENTER);
}

/* ============================================================
 * TCP Send
 * ============================================================
 */

SEC("kprobe/tcp_sendmsg")
int BPF_KPROBE(trace_tcp_sendmsg)
{
    return submit_event(EVENT_TCP_SENDMSG);
}

/* ============================================================
 * TCP Write
 * ============================================================
 */

SEC("kprobe/tcp_write_xmit")
int BPF_KPROBE(trace_tcp_write_xmit)
{
    return submit_event(EVENT_TCP_WRITE_XMIT);
}

/* ============================================================
 * IP Output
 * ============================================================
 */

SEC("kprobe/ip_output")
int BPF_KPROBE(trace_ip_output)
{
    return submit_event(EVENT_IP_OUTPUT);
}

/* ============================================================
 * NIC TX
 * ============================================================
 */

SEC("tracepoint/net/net_dev_queue")
int trace_net_dev_queue(struct trace_event_raw_net_dev_template *ctx)
{
    return submit_event(EVENT_NET_DEV_QUEUE);
}
