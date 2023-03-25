import abc
import logging
from datetime import datetime
from typing import Optional, Tuple, Any, Mapping, Dict

import requests
import hashlib
import hmac

from order_book import OrderBook, OrderType
from util import attach_file_handler, logger, \
    tojson, fromjson, record_timing


class MarketFeed(abc.ABC):
    def __init__(self) -> None:
        self.update_callbacks = []

        self.logger: logging.Logger = logging.getLogger(f"{type(self).__name__}")
        self.logger.setLevel(logging.INFO)

    @abc.abstractmethod
    async def subscribe_to_feed(self, ws):
        pass

    @abc.abstractmethod
    async def unsubscribe_to_feed(self, ws):
        pass

    @abc.abstractmethod
    def process_message(self, raw_msg: str) -> int:
        pass

    @property
    @abc.abstractmethod
    def API_URL(self) -> str:
        pass

    def attach_update_listener(self, callback):
        self.update_callbacks.append(callback)


class BinanceMaketFeed(MarketFeed):
    SECRET_API_KEY = "AfqGK6Jf8HQGiIXXXXXXXXXXXXXXXXXX4adhvcXeMSOSUKQEIkmIV9SmeZDu0kd5"
    API_KEY = "bD9QfIu4FBdRJpviWI075XXXXXXXXXXXXXXXXXXXlE4vcIbnFKQaeSm8f0vLW8te"
    SOCKET_API_URL = "wss://stream.binance.us:9443/ws"
    SNAPSHOT_API_URL = "https://www.binance.us/api/v1/depth"

    def __init__(self, orderbook: OrderBook) -> None:
        super().__init__()

        self.update_id: Optional[Tuple[int, int]] = None
        self.snapshot_update_id: Optional[int] = None
        self.message_id = 1
        self.stream = "depth"
        self.orderbook = orderbook
        self.products = [self.orderbook.product_id]
        self.no_subscription = True

        # TODO: create custom handler so all marketfeed related logs can go with one file
        #       with something denoting which marketfeed the log record belongs automatically
        #       added
        attach_file_handler(self.logger, "log_binance")

    @property
    def API_URL(self):
        product = self.products[0].lower()
        return f"{self.SOCKET_API_URL}/{product}@{self.stream}@100ms"

    @staticmethod
    def generate_subscribe_message(products, stream) -> Dict[str, Any]:
        request_body = {
            "method": "SUBSCRIBE",
            "params": [f"{product}@{stream}" for product in products],
        }

        return request_body

    @staticmethod
    def generate_unsubscribe_message(products, stream) -> Dict[str, Any]:
        request_body = {
            "method": "UNSUBSCRIBE",
            "params": [f"{product}@{stream}" for product in products],
        }

        return request_body

    def process_market_feed_snapshot(self):
        headers = {"x-mbx-apikey": self.API_KEY}
        product = self.products[0].upper()

        url = f"{self.SNAPSHOT_API_URL}?symbol={product}&limit=5000"
        res = requests.get(url, headers=headers)
        resj = res.json()
        if "code" in resj:
            self.logger.error(f"order book depth request to {url} failed with response: {resj}")
            raise Exception("failed to get order book snapshot")

        assert "bids" in resj and "asks" in resj
        update_id = int(resj["lastUpdateId"])
        self.snapshot_update_id = update_id

        bids, asks = resj["bids"], resj["asks"]

        self.logger.info(f"processing {len(bids) + len(asks)} updates in snapshot")
        self.process_ask_updates(asks, 0)  # just use 0 as the time stamp
        self.process_bid_updates(bids, 0)

    def process_bid_updates(self, updates: list, ts: int):
        for update in updates:
            price = float(update[0])
            quantity = float(update[1])

            self.orderbook.update_order(OrderType.BID, price, quantity, ts)

    def process_ask_updates(self, updates: list, ts: int):
        for update in updates:
            price = float(update[0])
            quantity = float(update[1])

            self.orderbook.update_order(OrderType.ASK, price, quantity, ts)

    async def subscribe_to_feed(self, ws):
        # don't need to subscribe to a stream for binance
        if self.no_subscription:
            # get orderbook snapshot
            self.process_market_feed_snapshot()
            return

        sub_msg = BinanceMaketFeed.generate_subscribe_message(self.products, stream=self.stream)
        sub_msg.update({"id": self.message_id})

        sub_msg_str = tojson(sub_msg)
        await ws.send(sub_msg_str)
        self.logger.info(f">>> {sub_msg_str}")

    async def unsubscribe_to_feed(self, ws):
        # don't need to subscribe to a stream for binance
        if self.no_subscription:
            return

        unsub_msg = BinanceMaketFeed.generate_unsubscribe_message(self.products, stream=self.stream)
        unsub_msg.update({"id": self.message_id})

        unsub_msg_str = tojson(unsub_msg)
        await ws.send(unsub_msg_str)
        self.logger.info(f">>> {unsub_msg_str}")

    def notify_update_callback(self):
        if not self.update_callbacks:
            return
        for callback in self.update_callbacks:
            callback(self.orderbook)

    @record_timing(name="binance process_message")
    def process_message(self, raw_msg: str) -> int:
        msg = fromjson(raw_msg)
        if "result" in msg and "id" in msg:
            self.logger.info(f"received success response {msg}")
            self.message_id += 1
            return 0

        if msg.get("e", None) != "depthUpdate":
            self.logger.error("ignoring unknown message: {msg}")
            return 0

        event_time = int(msg["E"])
        symbol = msg["s"]
        start_update_id = int(msg["U"])
        end_update_id = int(msg["u"])

        bids: list = msg["b"]
        asks: list = msg["a"]

        if not self.update_id:
            # no updates from socket have been processed yet, orderbook should only be updated with snapshot
            assert self.snapshot_update_id
            if not (start_update_id <= self.snapshot_update_id + 1 <= end_update_id):
                logger.debug(
                    f"snapshot update id: {self.snapshot_update_id}, ignoring update ({start_update_id}, {end_update_id})")
                return 0
        else:
            # ensure that message has correct update ids
            last_update_id = self.update_id[1]
            if not (start_update_id <= last_update_id + 1 <= end_update_id):
                logger.error(
                    f"last update id: {last_update_id}, got update with ids ({start_update_id}, {end_update_id})")
                # stop
                return 1000

        self.logger.info(f"processing {len(bids) + len(asks)} updates")
        self.update_id = (start_update_id, end_update_id)
        self.process_ask_updates(asks, event_time)
        self.process_bid_updates(bids, event_time)
        self.notify_update_callback()

        return 0


class CoinBaseMaketFeed(MarketFeed):
    SECRET_KEY = "kZetsXXXXXXXXyTjGV2La60JPaOxNL9L"
    API_KEY    = "LlnonXXXXXXbbud2"
    SOCKET_API_URL    = "wss://advanced-trade-ws.coinbase.com"
    NAME = "Coinbase"

    def __init__(self, orderbook: OrderBook) -> None:
        super().__init__()

        self.sequence_num = 0
        self.channel = "level2"
        self.orderbook = orderbook
        self.products = [self.orderbook.product_id]

        attach_file_handler(self.logger, "log_coinbase")

    @property
    def API_URL(self):
        return CoinBaseMaketFeed.SOCKET_API_URL

    @staticmethod
    def timestamp_and_sign(json_msg, channels, products):
        if type(products) != str:
            products = ','.join(products)
        if type(channels) != str:
            channels = ','.join(channels)

        ts = int(datetime.now().timestamp())
        signature_plain = f"{ts}{channels}{products}"

        signature = hmac.digest(bytes(CoinBaseMaketFeed.SECRET_KEY, 'utf-8'), bytes(signature_plain, 'utf-8'),
                                hashlib.sha256).hex()
        json_msg.update({
            'signature': signature,
            'timestamp': str(ts)
        })
        return json_msg

    @staticmethod
    def generate_subscribe_message(products, channel) -> Mapping[str, Any]:
        request_body = {
            "type": "subscribe",
            "channel": channel,
            "api_key": CoinBaseMaketFeed.API_KEY,
            "product_ids": products,
            "user_id": "",
        }

        return CoinBaseMaketFeed.timestamp_and_sign(request_body, channel, products)

    @staticmethod
    def generate_unsubscribe_message(products, channel) -> Mapping[str, Any]:
        request_body = {
            "type": "unsubscribe",
            "channel": channel,
            "api_key": CoinBaseMaketFeed.API_KEY,
            "product_ids": products,
            "user_id": "",
        }

        return CoinBaseMaketFeed.timestamp_and_sign(request_body, channel, products)

    async def subscribe_to_feed(self, ws):
        sub_msg = CoinBaseMaketFeed.generate_subscribe_message(self.products, channel=self.channel)
        sub_msg_str = tojson(sub_msg)
        await ws.send(sub_msg_str)
        self.logger.info(f">>> {sub_msg_str}")

    async def unsubscribe_to_feed(self, ws):
        unsub_msg = CoinBaseMaketFeed.generate_unsubscribe_message(self.products, channel=self.channel)
        unsub_msg_str = tojson(unsub_msg)
        await ws.send(unsub_msg_str)
        self.logger.info(f">>> {unsub_msg_str}")

    def process_updates(self, updates: list):
        for update in updates:
            side = update["side"]
            event_time = update["event_time"].rstrip("Z")
            price = float(update["price_level"])
            quantity = float(update["new_quantity"])

            if "." in event_time:
                date, seconds = event_time.split('.')
            else:
                date = event_time
                seconds = "0"

            unix_ts = datetime.strptime(date, "%Y-%m-%dT%H:%M:%S").timestamp() + float(f"0.{seconds}")

            if side == "bid":
                order_type = OrderType.BID
            elif side == "offer":
                order_type = OrderType.ASK
            else:
                self.logger.info(f"unknown side \"{side}\" in update event")
                continue

            self.orderbook.update_order(order_type, price, quantity, unix_ts)

    def process_snapshot_event(self, event: dict):
        product_id = event["product_id"]
        if product_id != self.orderbook.product_id:
            self.logger.info(f"ignoring event for {product_id}")
            return
        updates = event["updates"]
        self.logger.info(f"processing {len(updates)} updates in snapshot")

        self.process_updates(updates)

    def process_update_event(self, event: dict):
        product_id = event["product_id"]
        if product_id != self.orderbook.product_id:
            self.logger.info(f"ignoring event for {product_id}")
            return
        updates = event["updates"]
        self.logger.info(f"processing {len(updates)} updates")

        self.process_updates(updates)

    def notify_update_callback(self):
        if not self.update_callbacks:
            return
        for callback in self.update_callbacks:
            callback(self.orderbook)

    def process_order_book_update(self, msg):
        for event in msg["events"]:
            event_type = event["type"]
            if event_type == "snapshot":
                self.process_snapshot_event(event)
            elif event_type == "update":
                self.process_update_event(event)
                self.notify_update_callback()
            else:
                self.logger.info(f"unknown event type {event_type}, ignoring: {event}")

    # @record_timing(name="coinbase process_message")
    def process_message(self, raw_msg: str) -> int:
        msg = fromjson(raw_msg)
        if msg.get("type", None) == "error":
            self.logger.error(f"Error: {raw_msg}")
            return 1000

        if "sequence_num" in msg:
            seq = msg["sequence_num"]
            if seq != self.sequence_num:
                self.logger.error(f"excepting sequence_num {self.sequence_num}, got {seq}: {msg}")
                raise Exception("Unexpected message sequence number")
            self.sequence_num += 1

        channel = msg.get('channel', None)
        if channel == 'l2_data':
            self.process_order_book_update(msg)
        elif channel == 'subscriptions':
            pass
        else:
            # unknown channel ignore
            self.logger.info(f"ignoring message from channel {channel}")
        return 0
