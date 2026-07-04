from dataclasses import dataclass, field
import time
from typing import Dict


@dataclass
class Event:

    timestamp: float = field(default_factory=time.time)

    source: str = ""

    category: str = ""

    name: str = ""

    details: Dict = field(default_factory=dict)

    def __str__(self):

        return (
            f"[{self.timestamp:.6f}] "
            f"{self.source:12} "
            f"{self.category:10} "
            f"{self.name}"
        )