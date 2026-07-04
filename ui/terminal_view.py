from __future__ import annotations

from typing import Iterable

from timeline import Event


def render_timeline(events: Iterable[Event]) -> None:
    print("\n=== HTTP Flow Timeline ===")
    for event in events:
        print(f"[{event.layer}] {event.name} @ {event.timestamp:.3f} :: {event.details}")
    print("\n")
