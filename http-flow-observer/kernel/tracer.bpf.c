// SPDX-License-Identifier: GPL-2.0

#include "../vmlinux.h"

#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#include "event.bpf.h"

#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>

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
 * Connection Map
 * ============================================================
 */

struct connection
{
    __u64 socket;

    __u32 pid;

    __u32 tid;

    __u32 saddr;

    __u32 daddr;

    __u16 sport;

    __u16 dport;
};

struct
{
    __uint(type, BPF_MAP_TYPE_HASH);

    __uint(max_entries, 8192);

    __type(key, __u64);

    __type(value, struct connection);

} connections SEC(".maps");

/* ============================================================
 * Helper
 * ============================================================
 */

static __always_inline int read_socket(
    struct sock *sk,
    __u32 *saddr,
    __u32 *daddr,
    __u16 *sport,
    __u16 *dport)
{
    if (!sk)
        return 0;

    bpf_core_read(saddr,
                  sizeof(*saddr),
                  &sk->__sk_common.skc_rcv_saddr);

    bpf_core_read(daddr,
                  sizeof(*daddr),
                  &sk->__sk_common.skc_daddr);

    bpf_core_read(sport,
                  sizeof(*sport),
                  &sk->__sk_common.skc_num);

    bpf_core_read(dport,
                  sizeof(*dport),
                  &sk->__sk_common.skc_dport);

    *dport = __builtin_bswap16(*dport);

    return 0;
}

static __always_inline int submit_event_ex(
    __u32 type,
    __u32 saddr,
    __u32 daddr,
    __u16 sport,
    __u16 dport,
    __u16 packet_len,

    __u64 socket_cookie,
    __u64 socket_ptr,

    __u8 protocol,
    __u8 tcp_flags,
    __u32 seq,
    __u32 ack_seq)
{
    struct event *e;

    __u64 pid_tgid;
    __u64 uid_gid;

    e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
    if (!e)
        return 0;

    pid_tgid = bpf_get_current_pid_tgid();
    uid_gid  = bpf_get_current_uid_gid();

    e->timestamp = bpf_ktime_get_ns();

    e->pid = pid_tgid >> 32;
    e->tid = (__u32)pid_tgid;

    e->uid = (__u32)uid_gid;
    e->gid = uid_gid >> 32;

    e->cpu = bpf_get_smp_processor_id();

    e->event = type;

    e->saddr = saddr;
    e->daddr = daddr;

    e->sport = sport;
    e->dport = dport;

    e->packet_len = packet_len;
    e->socket_cookie = socket_cookie;
    e->socket_ptr = socket_ptr;

    e->protocol = protocol;
    e->tcp_flags = tcp_flags;

    e->seq = seq;
    e->ack_seq = ack_seq;

    bpf_get_current_comm(e->comm, sizeof(e->comm));

    bpf_ringbuf_submit(e, 0);

    return 0;
}

static __always_inline void
save_connection(struct sock *sk,
                __u32 pid,
                __u32 tid,
                __u32 saddr,
                __u32 daddr,
                __u16 sport,
                __u16 dport)
{
    struct connection conn = {};

    __u64 key;

    if (!sk)
        return;

    key = (__u64)sk;

    conn.socket = key;

    conn.pid = pid;

    conn.tid = tid;

    conn.saddr = saddr;

    conn.daddr = daddr;

    conn.sport = sport;

    conn.dport = dport;

    bpf_map_update_elem(
            &connections,
            &key,
            &conn,
            BPF_ANY);
}

/* ============================================================
 * Find Connection
 * ============================================================
 */


static __always_inline struct connection *
find_connection(struct sock *sk)
{
    __u64 key;

    if (!sk)
        return NULL;

    key = (__u64)sk;

    return bpf_map_lookup_elem(&connections, &key);
}

/* ============================================================
 * submit_event()
 * ============================================================
 */
static __always_inline int submit_event(__u32 type)
{
    return submit_event_ex(
        type,

        0,
        0,

        0,
        0,

        0,

        0,
        0,

        0,
        0,

        0,
        0);
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
int BPF_KPROBE(trace_tcp_v4_rcv,
               struct sk_buff *skb)
{
    struct sock *sk = NULL;

    __u32 saddr = 0;
    __u32 daddr = 0;

    __u16 sport = 0;
    __u16 dport = 0;

    __u64 socket_ptr = 0;

    bpf_core_read(&sk, sizeof(sk), &skb->sk);

    socket_ptr = (__u64)sk;

    read_socket(
        sk,
        &saddr,
        &daddr,
        &sport,
        &dport);
    __u64 pid_tgid;

    pid_tgid = bpf_get_current_pid_tgid();

    save_connection(
            sk,
            pid_tgid >> 32,
            (__u32)pid_tgid,
            saddr,
            daddr,
            sport,
            dport);

    return submit_event_ex(
        EVENT_TCP_V4_RCV,

        saddr,
        daddr,

        sport,
        dport,

        0,

        0,              /* socket_cookie */

        socket_ptr,

        6,

        0,

        0,

        0);
}

/* ============================================================
 * TCP Queue
 * ============================================================
 */

SEC("kprobe/tcp_data_queue")
int BPF_KPROBE(trace_tcp_data_queue, struct sock *sk)
{
    struct connection *conn;

    conn = find_connection(sk);

    if (!conn)
        return submit_event(EVENT_TCP_DATA_QUEUE);

    return submit_event_ex(
        EVENT_TCP_DATA_QUEUE,

        conn->saddr,
        conn->daddr,

        conn->sport,
        conn->dport,

        0,

        0,

        (__u64)sk,

        6,

        0,

        0,

        0);
}
/* ============================================================
 * Socket Readable
 * ============================================================
 */

SEC("kprobe/sock_def_readable")
int BPF_KPROBE(trace_sock_def_readable,
               struct sock *sk)
{
    struct connection *conn;

    conn = find_connection(sk);

    if (!conn)
        return submit_event(EVENT_SOCK_DEF_READABLE);

    return submit_event_ex(
            EVENT_SOCK_DEF_READABLE,

            conn->saddr,
            conn->daddr,

            conn->sport,
            conn->dport,

            0,

            0,

            (__u64)sk,

            6,

            0,

            0,

            0);
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
int BPF_KPROBE(trace_tcp_sendmsg,
               struct sock *sk)
{
    struct connection *conn;

    conn = find_connection(sk);

    if (!conn)
        return submit_event(EVENT_TCP_SENDMSG);

    return submit_event_ex(
            EVENT_TCP_SENDMSG,

            conn->saddr,
            conn->daddr,

            conn->sport,
            conn->dport,

            0,

            0,

            (__u64)sk,

            6,

            0,

            0,

            0);
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
int BPF_KPROBE(trace_ip_output,
               struct net *net,
               struct sock *sk)
{
    struct connection *conn;

    conn = find_connection(sk);

    if (!conn)
        return submit_event(EVENT_IP_OUTPUT);

    return submit_event_ex(
            EVENT_IP_OUTPUT,

            conn->saddr,
            conn->daddr,

            conn->sport,
            conn->dport,

            0,

            0,

            (__u64)sk,

            6,

            0,

            0,

            0);
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
