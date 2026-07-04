#ifndef __COMMON_H__
#define __COMMON_H__

#include <linux/types.h>

#define TASK_COMM_LEN 16

/* Event sent from eBPF kernel program to user space */
struct tcp_event {

    /* Kernel timestamp (nanoseconds) */
    __u64 timestamp_ns;

    /* Process ID */
    __u32 pid;

    /* Process name */
    char comm[TASK_COMM_LEN];

    /* IPv4 addresses (network byte order) */
    __u32 saddr;
    __u32 daddr;

    /* TCP ports (host byte order) */
    __u16 sport;
    __u16 dport;

};

#endif /* __COMMON_H__ */