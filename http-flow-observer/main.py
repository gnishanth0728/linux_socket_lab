from core.event import Event
from core.event_bus import EventBus
from core.timeline import Timeline
from core.logger import Logger


def main():

    Logger.info("HTTP Flow Observer")

    event_bus = EventBus()

    timeline = Timeline()

    Logger.info("Publishing sample events...")

    event_bus.publish(
        Event(
            source="controller",
            category="system",
            name="program-start",
        )
    )

    event_bus.publish(
        Event(
            source="controller",
            category="system",
            name="initialization-complete",
        )
    )

    for event in event_bus.consume():

        timeline.add(event)

    timeline.sort()

    timeline.print()

    Logger.info("Framework initialized successfully.")


if __name__ == "__main__":

    main()