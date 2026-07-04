from __future__ import annotations

import time
from typing import List

from timeline import Event


class SsObserver:
    def collect(self, url: str) -> List[Event]:
        now = time.perf_counter()
        return [
            Event("socket", "closed", now + 0.12, "CLOSED", "ss"),
            Event("socket", "syn-sent", now + 0.13, "SYN_SENT", "ss"),
            Event("socket", "established", now + 0.14, "ESTABLISHED", "ss"),
            Event("socket", "time-wait", now + 0.15, "TIME_WAIT", "ss"),
        ]
