#ifndef HTTP_FLOW_EVENT_BPF_H
#define HTTP_FLOW_EVENT_BPF_H

#include "../vmlinux.h"

#define TASK_COMM_LEN 16

enum event_type
{
    EVENT_UNKNOWN = 0,

    /* =========================================================
     * Network Receive
     * ========================================================= */

    EVENT_NET_RX,

    EVENT_IRQ_ENTRY,

    EVENT_SOFTIRQ_ENTRY,

    EVENT_IP_RCV,

    EVENT_TCP_V4_RCV,

    EVENT_TCP_DATA_QUEUE,

    EVENT_SOCK_DEF_READABLE,

    /* =========================================================
     * Socket
     * ========================================================= */

    EVENT_ACCEPT4_ENTER,

    EVENT_ACCEPT4_EXIT,

    EVENT_RECVFROM_ENTER,

    EVENT_RECVFROM_EXIT,

    EVENT_SENDTO_ENTER,

    EVENT_SENDTO_EXIT,

    /* =========================================================
     * Network Transmit
     * ========================================================= */

    EVENT_TCP_SENDMSG,

    EVENT_TCP_WRITE_XMIT,

    EVENT_IP_OUTPUT,

    EVENT_NET_DEV_QUEUE
};

struct event
{
    /* Timestamp */

    __u64 timestamp;

    /* Process */

    __u32 pid;

    __u32 tid;

    __u32 uid;

    __u32 gid;

    /* CPU */

    __u32 cpu;

    /* Event */

    __u32 event;

    /* Process Name */

    char comm[TASK_COMM_LEN];
};

#endif /* HTTP_FLOW_EVENT_BPF_H */