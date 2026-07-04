from __future__ import annotations

from typing import Iterable, List

from timeline import Event


def parse_strace_log(lines: Iterable[str]) -> List[Event]:
    events: List[Event] = []
    for line in lines:
        if line.strip():
            events.append(Event("syscall", "syscall", 0.0, line.strip(), "strace-parser"))
    return events
