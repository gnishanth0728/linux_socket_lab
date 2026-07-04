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