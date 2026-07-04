from __future__ import annotations

import time
from typing import List

from timeline import Event


class MitmproxyObserver:
    def collect(self, url: str) -> List[Event]:
        now = time.perf_counter()
        return [
            Event("http", "mitmproxy-request", now + 0.10, f"Captured request for {url}", "mitmproxy"),
            Event("http", "mitmproxy-response", now + 0.11, "Captured response body", "mitmproxy"),
        ]
