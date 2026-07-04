#ifndef HTTP_FLOW_COMMON_BPF_H
#define HTTP_FLOW_COMMON_BPF_H

#include "../vmlinux.h"

#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_tracing.h>

#include "../collector/event.h"

/* Ring Buffer */

struct
{
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 24);
} events SEC(".maps");

/* Common Event Submitter */

static __always_inline int submit_event(__u32 event_type)
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

    e->pid  = pid_tgid >> 32;
    e->tid  = (__u32)pid_tgid;

    e->uid  = (__u32)uid_gid;
    e->gid  = uid_gid >> 32;

    e->cpu  = bpf_get_smp_processor_id();

    e->ppid = 0;

    e->event = event_type;

    e->reserved = 0;

    bpf_get_current_comm(e->comm, sizeof(e->comm));

    bpf_ringbuf_submit(e, 0);

    return 0;
}

#endif