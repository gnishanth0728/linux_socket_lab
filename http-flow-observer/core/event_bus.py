from queue import Queue


class EventBus:

    def __init__(self):

        self.queue = Queue()

    def publish(self, event):

        self.queue.put(event)

    def consume(self):

        while not self.queue.empty():

            yield self.queue.get()