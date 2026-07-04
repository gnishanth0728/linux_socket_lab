// SPDX-License-Identifier: GPL-2.0

#include "common.bpf.h"

char LICENSE[] SEC("license") = "GPL";

/* =========================================================================
 * IP Output
 * Function: ip_output()
 * =========================================================================
 */

SEC("kprobe/ip_output")
int BPF_KPROBE(trace_ip_output)
{
    return submit_event(EVENT_IP_OUTPUT);
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

/* =========================================================================
 * Network Device Queue
 * Tracepoint: net:net_dev_queue
 * =========================================================================
 */

SEC("tracepoint/net/net_dev_queue")
int trace_net_dev_queue(struct trace_event_raw_net_dev_template *ctx)
{
    return submit_event(EVENT_NET_DEV_QUEUE);
}