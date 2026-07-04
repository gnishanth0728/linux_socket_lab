from __future__ import annotations

from dataclasses import dataclass, asdict
from typing import List


@dataclass
class Event:
    layer: str
    name: str
    timestamp: float
    details: str = ""
    source: str = ""

    def to_dict(self) -> dict:
        return asdict(self)


class TimelineBuilder:
    def __init__(self) -> None:
        self._events: List[Event] = []

    def add(self, event: Event) -> None:
        self._events.append(event)

    def build(self) -> List[Event]:
        return sorted(self._events, key=lambda event: event.timestamp)

    def to_dicts(self) -> List[dict]:
        return [event.to_dict() for event in self.build()]
