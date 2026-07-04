from __future__ import annotations

from typing import Iterable, List

from timeline import Event


def parse_tcpdump_log(lines: Iterable[str]) -> List[Event]:
    events: List[Event] = []
    for line in lines:
        if line.strip():
            events.append(Event("tcp", "packet", 0.0, line.strip(), "tcpdump-parser"))
    return events
