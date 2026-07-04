// SPDX-License-Identifier: GPL-2.0

#include "common.bpf.h"

char LICENSE[] SEC("license") = "GPL";

/* =========================================================================
 * accept4() Entry
 * =========================================================================
 */

SEC("tracepoint/syscalls/sys_enter_accept4")
int trace_accept4_enter(struct trace_event_raw_sys_enter *ctx)
{
    return submit_event(EVENT_ACCEPT4_ENTER);
}

/* =========================================================================
 * accept4() Exit
 * =========================================================================
 */

SEC("tracepoint/syscalls/sys_exit_accept4")
int trace_accept4_exit(struct trace_event_raw_sys_exit *ctx)
{
    return submit_event(EVENT_ACCEPT4_EXIT);
}

/* =========================================================================
 * recvfrom() Entry
 * =========================================================================
 */

SEC("tracepoint/syscalls/sys_enter_recvfrom")
int trace_recvfrom_enter(struct trace_event_raw_sys_enter *ctx)
{
    return submit_event(EVENT_RECVFROM_ENTER);
}

/* =========================================================================
 * recvfrom() Exit
 * =========================================================================
 */

SEC("tracepoint/syscalls/sys_exit_recvfrom")
int trace_recvfrom_exit(struct trace_event_raw_sys_exit *ctx)
{
    return submit_event(EVENT_RECVFROM_EXIT);
}

/* =========================================================================
 * sendto() Entry
 * =========================================================================
 */

SEC("tracepoint/syscalls/sys_enter_sendto")
int trace_sendto_enter(struct trace_event_raw_sys_enter *ctx)
{
    return submit_event(EVENT_SENDTO_ENTER);
}

/* =========================================================================
 * sendto() Exit
 * =========================================================================
 */

SEC("tracepoint/syscalls/sys_exit_sendto")
int trace_sendto_exit(struct trace_event_raw_sys_exit *ctx)
{
    return submit_event(EVENT_SENDTO_EXIT);
}