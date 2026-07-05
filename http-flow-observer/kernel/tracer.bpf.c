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

struct
{
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 16384);
    __type(key, __u32);
    __type(value, __u64);
} task_sockets SEC(".maps");

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

static __always_inline void remember_task_socket(__u64 socket_ptr)
{
    __u64 pid_tgid;
    __u32 tid;

    if (!socket_ptr)
        return;

    pid_tgid = bpf_get_current_pid_tgid();
    tid = (__u32)pid_tgid;

    bpf_map_update_elem(&task_sockets, &tid, &socket_ptr, BPF_ANY);
}

static __always_inline int submit_socket_event(__u32 type, struct sock *sk)
{
    struct connection *conn;
    __u32 saddr = 0;
    __u32 daddr = 0;
    __u16 sport = 0;
    __u16 dport = 0;

    if (!sk)
        return submit_event(type);

    remember_task_socket((__u64)sk);

    conn = find_connection(sk);
    if (conn)
    {
        saddr = conn->saddr;
        daddr = conn->daddr;
        sport = conn->sport;
        dport = conn->dport;
    }
    else
    {
        read_socket(sk, &saddr, &daddr, &sport, &dport);
    }

    return submit_event_ex(
        type,
        saddr,
        daddr,
        sport,
        dport,
        0,
        0,
        (__u64)sk,
        6,
        0,
        0,
        0);
}

static __always_inline int submit_current_task_event(__u32 type)
{
    __u64 pid_tgid;
    __u32 tid;
    __u64 *socket_ptr;
    struct connection *conn;
    struct sock *sk;

    pid_tgid = bpf_get_current_pid_tgid();
    tid = (__u32)pid_tgid;

    socket_ptr = bpf_map_lookup_elem(&task_sockets, &tid);
    if (!socket_ptr || !*socket_ptr)
        return submit_event(type);

    sk = (struct sock *)(unsigned long)(*socket_ptr);
    conn = find_connection(sk);

    if (!conn)
        return submit_event_ex(type, 0, 0, 0, 0, 0, 0, *socket_ptr, 6, 0, 0, 0);

    return submit_event_ex(
        type,
        conn->saddr,
        conn->daddr,
        conn->sport,
        conn->dport,
        0,
        0,
        *socket_ptr,
        6,
        0,
        0,
        0);
}

SEC("uprobe//usr/sbin/nginx:ngx_http_process_request_line")
int uprobe_ngx_http_process_request_line(void *ctx)
{
    return submit_current_task_event(EVENT_NGINX_HTTP_PARSE);
}

SEC("uprobe//usr/sbin/nginx:ngx_http_upstream_init_request")
int uprobe_ngx_http_upstream_init_request(void *ctx)
{
    return submit_current_task_event(EVENT_NGINX_REVERSE_PROXY);
}

SEC("uprobe//usr/sbin/nginx:ngx_http_upstream_send_request")
int uprobe_ngx_http_upstream_send_request(void *ctx)
{
    return submit_current_task_event(EVENT_NGINX_BACKEND_SOCKET);
}

SEC("uprobe//usr/sbin/nginx:ngx_http_finalize_request")
int uprobe_ngx_http_finalize_request(void *ctx)
{
    return submit_current_task_event(EVENT_NGINX_RESPONSE_GEN);
}

SEC("uprobe//usr/sbin/nginx:ngx_http_writer")
int uprobe_ngx_http_writer(void *ctx)
{
    return submit_current_task_event(EVENT_NGINX_RESPONSE_TX);
}

SEC("tracepoint/syscalls/sys_enter_accept4")
int trace_accept4_enter(struct trace_event_raw_sys_enter *ctx)
{
    return submit_event(EVENT_ACCEPT4_ENTER);
}

SEC("tracepoint/syscalls/sys_exit_accept4")
int trace_accept4_exit(struct trace_event_raw_sys_exit *ctx)
{
    return submit_event(EVENT_ACCEPT4_EXIT);
}

SEC("tracepoint/syscalls/sys_enter_recvfrom")
int trace_recvfrom_enter(struct trace_event_raw_sys_enter *ctx)
{
    return submit_event(EVENT_RECVFROM_ENTER);
}

SEC("tracepoint/syscalls/sys_exit_recvfrom")
int trace_recvfrom_exit(struct trace_event_raw_sys_exit *ctx)
{
    return submit_event(EVENT_RECVFROM_EXIT);
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

SEC("kprobe/napi_gro_receive")
int BPF_KPROBE(trace_napi_gro_receive, struct napi_struct *napi, struct sk_buff *skb)
{
    struct sock *sk = NULL;

    if (skb)
        bpf_core_read(&sk, sizeof(sk), &skb->sk);

    return submit_socket_event(EVENT_NAPI_POLL, sk);
}

SEC("kprobe/eth_type_trans")
int BPF_KPROBE(trace_eth_type_trans, struct sk_buff *skb)
{
    struct sock *sk = NULL;

    if (skb)
        bpf_core_read(&sk, sizeof(sk), &skb->sk);

    return submit_socket_event(EVENT_ETHERNET_RX, sk);
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

SEC("kprobe/nf_hook_slow")
int BPF_KPROBE(trace_nf_hook_slow, struct sk_buff *skb)
{
    struct sock *sk = NULL;

    if (skb)
        bpf_core_read(&sk, sizeof(sk), &skb->sk);

    return submit_socket_event(EVENT_NETFILTER_HOOK, sk);
}

SEC("kprobe/ip_route_input_noref")
int BPF_KPROBE(trace_ip_route_input_noref, struct sk_buff *skb)
{
    struct sock *sk = NULL;

    if (skb)
        bpf_core_read(&sk, sizeof(sk), &skb->sk);

    return submit_socket_event(EVENT_ROUTE_LOOKUP, sk);
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
    return submit_socket_event(EVENT_TCP_DATA_QUEUE, sk);
}

SEC("kprobe/tcp_rcv_established")
int BPF_KPROBE(trace_tcp_rcv_established, struct sock *sk)
{
    return submit_socket_event(EVENT_TCP_STATE_MACHINE, sk);
}
/* ============================================================
 * Socket Readable
 * ============================================================
 */

SEC("kprobe/sock_def_readable")
int BPF_KPROBE(trace_sock_def_readable,
               struct sock *sk)
{
    return submit_socket_event(EVENT_SOCK_DEF_READABLE, sk);
}

SEC("tracepoint/sched/sched_wakeup")
int trace_sched_wakeup(void *ctx)
{
    return submit_event(EVENT_SCHED_WAKEUP);
}

SEC("tracepoint/sched/sched_switch")
int trace_sched_switch(void *ctx)
{
    return submit_event(EVENT_SCHED_SWITCH);
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

SEC("tracepoint/syscalls/sys_exit_sendto")
int trace_sendto_exit(struct trace_event_raw_sys_exit *ctx)
{
    return submit_event(EVENT_SENDTO_EXIT);
}

/* ============================================================
 * TCP Send
 * ============================================================
 */

SEC("kprobe/tcp_sendmsg")
int BPF_KPROBE(trace_tcp_sendmsg,
               struct sock *sk)
{
    return submit_socket_event(EVENT_TCP_SENDMSG, sk);
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
    return submit_socket_event(EVENT_IP_OUTPUT, sk);
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
