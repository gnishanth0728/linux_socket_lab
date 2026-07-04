#ifndef HTTP_FLOW_EVENT_H
#define HTTP_FLOW_EVENT_H

#include <linux/types.h>

#define TASK_COMM_LEN 16
#define MAX_STAGE_LEN 32
#define MAX_EVENT_LEN 64

/* Event Types */
enum event_type
{
    EVENT_UNKNOWN = 0,

    /* Ingress */
    EVENT_NET_RX,
    EVENT_IRQ_ENTRY,
    EVENT_SOFTIRQ_NET_RX,
    EVENT_IP_RCV,
    EVENT_TCP_V4_RCV,
    EVENT_TCP_DATA_QUEUE,
    EVENT_SOCK_DEF_READABLE,

    /* Socket */
    EVENT_ACCEPT4_ENTER,
    EVENT_ACCEPT4_EXIT,
    EVENT_RECVFROM_ENTER,
    EVENT_RECVFROM_EXIT,
    EVENT_SENDTO_ENTER,
    EVENT_SENDTO_EXIT,

    /* Egress */
    EVENT_TCP_SENDMSG,
    EVENT_TCP_WRITE_XMIT,
    EVENT_IP_OUTPUT,
    EVENT_NET_DEV_QUEUE
};

/* Shared Event Structure */
struct event
{
    __u64 timestamp;

    __u32 pid;
    __u32 tid;
    __u32 ppid;

    __u32 uid;
    __u32 gid;

    __u32 cpu;

    __u32 event;

    __u32 reserved;

    char comm[TASK_COMM_LEN];
};

#endif