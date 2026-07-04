from __future__ import annotations

import time
from typing import List

from timeline import Event


class PerfObserver:
    def collect(self, url: str) -> List[Event]:
        now = time.perf_counter()
        return [
            Event("kernel", "perf-sample", now + 0.19, "perf sample captured", "perf"),
        ]
