from __future__ import annotations

import time
from typing import List

from timeline import Event


class StraceObserver:
    def collect(self, url: str) -> List[Event]:
        now = time.perf_counter()
        return [
            Event("syscall", "socket", now + 0.01, "socket() invoked", "strace"),
            Event("syscall", "connect", now + 0.02, "connect() invoked", "strace"),
            Event("syscall", "send", now + 0.03, "send() invoked", "strace"),
            Event("syscall", "recv", now + 0.04, "recv() invoked", "strace"),
            Event("syscall", "close", now + 0.05, "close() invoked", "strace"),
        ]
