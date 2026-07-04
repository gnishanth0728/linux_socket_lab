// SPDX-License-Identifier: GPL-2.0

#include "common.bpf.h"

char LICENSE[] SEC("license") = "GPL";

/* =========================================================================
 * Hardware Interrupt Entry
 * Tracepoint: irq:irq_handler_entry
 * =========================================================================
 */

SEC("tracepoint/irq/irq_handler_entry")
int trace_irq_handler_entry(struct trace_event_raw_irq_handler_entry *ctx)
{
    return submit_event(EVENT_IRQ_ENTRY);
}

/* =========================================================================
 * NET_RX SoftIRQ
 * Tracepoint: irq:softirq_entry
 * =========================================================================
 */

SEC("tracepoint/irq/softirq_entry")
int trace_softirq_entry(struct trace_event_raw_softirq *ctx)
{
    /* NET_RX_SOFTIRQ = 3 */
    if (ctx->vec != 3)
        return 0;

    return submit_event(EVENT_SOFTIRQ_NET_RX);
}