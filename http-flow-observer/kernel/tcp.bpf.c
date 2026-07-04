// SPDX-License-Identifier: GPL-2.0

#include "common.bpf.h"

char LICENSE[] SEC("license") = "GPL";

/* =========================================================================
 * TCP Receive
 * Function: tcp_v4_rcv()
 * =========================================================================
 */

SEC("kprobe/tcp_v4_rcv")
int BPF_KPROBE(trace_tcp_v4_rcv)
{
    return submit_event(EVENT_TCP_V4_RCV);
}

/* =========================================================================
 * TCP Data Queue
 * Function: tcp_data_queue()
 * =========================================================================
 */

SEC("kprobe/tcp_data_queue")
int BPF_KPROBE(trace_tcp_data_queue)
{
    return submit_event(EVENT_TCP_DATA_QUEUE);
}

/* =========================================================================
 * TCP Send Message
 * Function: tcp_sendmsg()
 * =========================================================================
 */

SEC("kprobe/tcp_sendmsg")
int BPF_KPROBE(trace_tcp_sendmsg)
{
    return submit_event(EVENT_TCP_SENDMSG);
}

/* =========================================================================
 * TCP Segmentation
 * Function: tcp_write_xmit()
 * =========================================================================
 */

SEC("kprobe/tcp_write_xmit")
int BPF_KPROBE(trace_tcp_write_xmit)
{
    return submit_event(EVENT_TCP_WRITE_XMIT);
}