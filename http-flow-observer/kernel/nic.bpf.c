// SPDX-License-Identifier: GPL-2.0

#include "common.bpf.h"

char LICENSE[] SEC("license") = "GPL";

/* =========================================================================
 * Ethernet Frame Received
 * Tracepoint: net:netif_receive_skb
 * =========================================================================
 */

SEC("tracepoint/net/netif_receive_skb")
int trace_netif_receive_skb(struct trace_event_raw_net_dev_template *ctx)
{
    return submit_event(EVENT_NET_RX);
}

/* =========================================================================
 * Ethernet Frame Queued For Transmission
 * Tracepoint: net:net_dev_queue
 * =========================================================================
 */

SEC("tracepoint/net/net_dev_queue")
int trace_net_dev_queue(struct trace_event_raw_net_dev_template *ctx)
{
    return submit_event(EVENT_NET_DEV_QUEUE);
}