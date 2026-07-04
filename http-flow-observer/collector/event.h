#ifndef HTTP_FLOW_EVENT_H
#define HTTP_FLOW_EVENT_H

#include <stdint.h>

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
    uint64_t timestamp;

    uint32_t pid;

    uint32_t tid;

    uint32_t uid;

    uint32_t gid;

    uint32_t cpu;

    uint32_t event;

    uint32_t saddr;

    uint32_t daddr;

    uint16_t sport;

    uint16_t dport;

    uint16_t packet_len;

    uint64_t socket_cookie;

    uint64_t socket_ptr;

    uint8_t protocol;

    uint8_t tcp_flags;

    uint32_t seq;

    uint32_t ack_seq;

    char comm[TASK_COMM_LEN];
};

#endif