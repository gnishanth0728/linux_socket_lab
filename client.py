from __future__ import annotations

import time
import urllib.request
from typing import List

from timeline import Event, TimelineBuilder


def perform_http_request(url: str, timeout: int = 5) -> List[Event]:
    builder = TimelineBuilder()
    start = time.perf_counter()
    builder.add(Event("application", "request-start", start, f"Preparing request to {url}", "client"))

    request = urllib.request.Request(
        url,
        headers={"User-Agent": "http-observer/1.0"},
    )

    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            body = response.read().decode("utf-8", errors="replace")
            status_code = response.getcode()
            headers = dict(response.headers.items())
            end = time.perf_counter()
            builder.add(
                Event(
                    "http",
                    "response-received",
                    end,
                    f"HTTP {status_code} {body[:120]}",
                    "client",
                )
            )
            builder.add(
                Event(
                    "application",
                    "response-body",
                    end,
                    f"body-bytes={len(body)} headers={len(headers)}",
                    "client",
                )
            )
    except Exception as exc:  # pragma: no cover - network errors are handled at runtime
        end = time.perf_counter()
        builder.add(Event("http", "request-failed", end, str(exc), "client"))

    return builder.build()
