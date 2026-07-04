from abc import ABC, abstractmethod


class Observer(ABC):

    def __init__(self, event_bus):

        self.event_bus = event_bus

    @abstractmethod
    def start(self):
        pass

    @abstractmethod
    def stop(self):
        pass