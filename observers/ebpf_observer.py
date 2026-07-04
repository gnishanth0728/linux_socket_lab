from __future__ import annotations

import time
from typing import List

from timeline import Event


class EbpfObserver:
    def collect(self, url: str) -> List[Event]:
        now = time.perf_counter()
        return [
            Event("kernel", "tcp_sendmsg", now + 0.16, "tcp_sendmsg traced", "ebpf"),
            Event("kernel", "tcp_recvmsg", now + 0.17, "tcp_recvmsg traced", "ebpf"),
            Event("kernel", "ip_queue_xmit", now + 0.18, "ip_queue_xmit traced", "ebpf"),
        ]
