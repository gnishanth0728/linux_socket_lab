#ifndef HTTP_FLOW_EVENT_BPF_H
#define HTTP_FLOW_EVENT_BPF_H

#include "../vmlinux.h"

#define TASK_COMM_LEN 16

enum event_type
{
    EVENT_UNKNOWN = 0,

    EVENT_NET_RX,
    EVENT_IRQ_ENTRY,
    EVENT_SOFTIRQ_ENTRY,
    EVENT_IP_RCV,
    EVENT_TCP_V4_RCV,
    EVENT_TCP_DATA_QUEUE,
    EVENT_SOCK_DEF_READABLE,

    EVENT_ACCEPT4_ENTER,
    EVENT_ACCEPT4_EXIT,
    EVENT_RECVFROM_ENTER,
    EVENT_RECVFROM_EXIT,
    EVENT_SENDTO_ENTER,
    EVENT_SENDTO_EXIT,

    EVENT_TCP_SENDMSG,
    EVENT_TCP_WRITE_XMIT,
    EVENT_IP_OUTPUT,
    EVENT_NET_DEV_QUEUE
};

struct event
{
    __u64 timestamp;

    __u32 pid;

    __u32 tid;

    __u32 uid;

    __u32 gid;

    __u32 cpu;

    __u32 event;

    __u32 saddr;

    __u32 daddr;

    __u16 sport;

    __u16 dport;

    __u16 packet_len;

    __u64 socket_cookie;

    __u64 socket_ptr;

    __u8 protocol;

    __u8 tcp_flags;

    __u32 seq;

    __u32 ack_seq;

    char comm[TASK_COMM_LEN];
};

#endif