class Timeline:

    def __init__(self):

        self.events = []

    def add(self, event):

        self.events.append(event)

    def sort(self):

        self.events.sort(key=lambda e: e.timestamp)

    def print(self):

        print()
        print("=" * 80)
        print("TIMELINE")
        print("=" * 80)

        for event in self.events:
            print(event)

        print("=" * 80)