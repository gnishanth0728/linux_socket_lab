import unittest

from timeline import Event, TimelineBuilder


class TimelineBuilderTests(unittest.TestCase):
    def test_timeline_builder_sorts_events_chronologically(self):
        builder = TimelineBuilder()
        builder.add(Event("syscall", "socket", timestamp=3.0, details="socket()"))
        builder.add(Event("http", "response", timestamp=1.0, details="HTTP/1.1 200 OK"))
        builder.add(Event("tcp", "connect", timestamp=2.0, details="SYN_SENT"))

        events = builder.build()

        self.assertEqual([event.name for event in events], ["response", "connect", "socket"])


if __name__ == "__main__":
    unittest.main()
