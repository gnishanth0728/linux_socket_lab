#ifndef __COMMON_H__
#define __COMMON_H__

#ifdef __BPF__

#define TASK_COMM_LEN 16

#else

#include <linux/types.h>

#define TASK_COMM_LEN 16

#endif

struct tcp_event {

    __u64 timestamp_ns;

    __u32 pid;

    char comm[TASK_COMM_LEN];

    __u32 saddr;

    __u32 daddr;

    __u16 sport;

    __u16 dport;
};

#endif