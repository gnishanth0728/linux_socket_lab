from __future__ import annotations

import time
from typing import List

from timeline import Event


class TcpdumpObserver:
    def collect(self, url: str) -> List[Event]:
        now = time.perf_counter()
        return [
            Event("tcp", "syn", now + 0.06, "SYN packet observed", "tcpdump"),
            Event("tcp", "ack", now + 0.07, "ACK packet observed", "tcpdump"),
            Event("tcp", "psh", now + 0.08, "PSH packet observed", "tcpdump"),
            Event("tcp", "fin", now + 0.09, "FIN packet observed", "tcpdump"),
        ]
