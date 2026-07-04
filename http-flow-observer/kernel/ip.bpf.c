// SPDX-License-Identifier: GPL-2.0

#include "common.bpf.h"

char LICENSE[] SEC("license") = "GPL";

/* =========================================================================
 * IPv4 Receive
 * Function: ip_rcv()
 * =========================================================================
 */

SEC("kprobe/ip_rcv")
int BPF_KPROBE(trace_ip_rcv)
{
    return submit_event(EVENT_IP_RCV);
}

/* =========================================================================
 * Socket Readable Notification
 * Function: sock_def_readable()
 * =========================================================================
 */

SEC("kprobe/sock_def_readable")
int BPF_KPROBE(trace_sock_def_readable)
{
    return submit_event(EVENT_SOCK_DEF_READABLE);
}