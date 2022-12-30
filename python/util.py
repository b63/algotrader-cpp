from typing import Optional

import os
import logging
import time
import json


def attach_file_handler(logger: logging.Logger, filename: str, fmt: Optional[str] = None, directory="logs"):
    if fmt:
        fmt = "[%(funcName)s:%(lineno)d] %(message)s"
    handler = logging.FileHandler(filename=os.path.join(directory, filename))
    handler.setFormatter(logging.Formatter(fmt=fmt))

    logger.propagate = False
    logger.addHandler(handler)


logging.basicConfig(filename="logs/log", format="[%(funcName)s:%(lineno)d] %(message)s")
tuilogger = logging.getLogger("tui")
tradelogger = logging.getLogger("trade")
logger = logging.getLogger("general")

attach_file_handler(tuilogger, "log_tui")
attach_file_handler(tradelogger, "log_trade")

tuilogger.setLevel(logging.DEBUG)
logger.setLevel(logging.INFO)
tradelogger.setLevel(logging.INFO)


def logcall(func, log=None, log_level=logging.INFO, before=True):
    if not log:
        log = logger

    def inner(*args, **kwargs):
        args_str = ", ".join(map(str, args))
        kwargs_str = f", {kwargs}".strip("{}") if kwargs else ""
        if before:
            log.log(log_level, f"INVOKING {func.__name__}({args_str}{kwargs_str})")
        ret = func(*args, **kwargs)
        if before:
            log.log(log_level, f"RETURNED {func.__name__} -> {ret}")
        else:
            log.log(log_level, f"INVOKED {func.__name__}({args}{kwargs_str}) -> {ret}")

        return ret

    return inner


def record_timing(name):
    def decorator(func):
        prev = 0

        def inner(*args, **kwargs):
            nonlocal prev
            start = time.time()
            ret = func(*args, **kwargs)
            end = time.time()

            delta = end - start
            if prev != 0:
                latency = start - prev
                logger.info(f"<{name}> execution time: {delta * 1000:,.6f} ms, latency: {latency * 1000:,.6f}ms")
            else:
                logger.info(f"<{name}> execution time: {delta * 1000:,.6f} ms")

            prev = time.time()
            return ret

        return inner

    return decorator


def tojson(message):
    return json.dumps(message, separators=(",", ":"))


def fromjson(message):
    return json.loads(message)
