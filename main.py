from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
from typing import List

from client import perform_http_request
from observers.ebpf_observer import EbpfObserver
from observers.mitmproxy_observer import MitmproxyObserver
from observers.perf_observer import PerfObserver
from observers.ss_observer import SsObserver
from observers.strace_observer import StraceObserver
from observers.tcpdump_observer import TcpdumpObserver
from timeline import Event, TimelineBuilder
from ui.html_report import write_html_report
from ui.terminal_view import render_timeline


def build_observers() -> List[object]:
    return [
        StraceObserver(),
        TcpdumpObserver(),
        MitmproxyObserver(),
        SsObserver(),
        EbpfObserver(),
        PerfObserver(),
    ]


def run_observation(url: str, output_path: str = "reports/http_observer.html") -> List[Event]:
    events = [*perform_http_request(url)]

    for observer in build_observers():
        events.extend(observer.collect(url))

    builder = TimelineBuilder()
    for event in events:
        builder.add(event)

    sorted_events = builder.build()

    output_file = Path(output_path)
    output_file.parent.mkdir(parents=True, exist_ok=True)

    write_html_report(sorted_events, output_file)

    logs_dir = Path("logs")
    logs_dir.mkdir(exist_ok=True)
    with (logs_dir / "timeline.json").open("w", encoding="utf-8") as handle:
        json.dump([event.to_dict() for event in sorted_events], handle, indent=2)

    render_timeline(sorted_events)
    return sorted_events


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="HTTP Flow Observer")
    parser.add_argument("--url", default="http://127.0.0.1:8000/", help="Target URL")
    parser.add_argument("--output", default="reports/http_observer.html", help="HTML output path")
    return parser.parse_args()


if __name__ == "__main__":
    args = parse_args()
    run_observation(args.url, args.output)
