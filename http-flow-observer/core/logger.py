import datetime


class Logger:

    @staticmethod
    def info(message):

        now = datetime.datetime.now().strftime("%H:%M:%S")

        print(f"[INFO {now}] {message}")

    @staticmethod
    def warning(message):

        now = datetime.datetime.now().strftime("%H:%M:%S")

        print(f"[WARN {now}] {message}")

    @staticmethod
    def error(message):

        now = datetime.datetime.now().strftime("%H:%M:%S")

        print(f"[ERROR {now}] {message}")