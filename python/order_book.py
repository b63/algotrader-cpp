from typing import Optional, Sequence, Set, List, Dict, Tuple
from enum import Enum


class ordered_set:
    def __init__(self, iterable: Optional[Sequence[float]] = None) -> None:
        if not iterable:
            self._array = []
            self._set = set()
            return

        self._set: Set[float] = set(iterable)
        self._array: List[float] = list(self._set)
        self._array.sort()

    def __getitem__(self, index: int) -> float:
        return self._array[index]

    def __contains__(self, key: float) -> bool:
        return key in self._set

    def __len__(self) -> int:
        return len(self._array)

    def _find_bin(self, key: float) -> int:
        n = len(self._array)
        start = 0
        while n > 1:
            left = (n + 1) // 2
            right = n - left
            mid = start + left - 1
            if key > self._array[mid]:
                start = mid + 1
                n = right
            else:
                n = left

        return start

    def indexof(self, key: float) -> int:
        return self._find_bin(key)

    def add(self, key: float):
        if key in self._set:
            return

        if len(self._array) == 0:
            self._array.append(key)
        else:
            ind = self._find_bin(key)
            if key < self._array[ind]:
                self._array.insert(ind, key)
            else:
                self._array.insert(ind + 1, key)

        self._set.add(key)

    def remove(self, key: float):
        if key not in self._set:
            return

        ind = self._find_bin(key)
        assert self._array[ind] == key

        self._array.pop(ind)
        self._set.remove(key)


class OrderType(Enum):
    BID = 0
    ASK = 1


class OrderBook:
    def __init__(self, product_id: str, name: str) -> None:
        self.__product_id = product_id
        self.name = name

        # buy orders: price -> (quantity, unix time stamp)
        self.bids: Dict[float, Tuple[float, float]] = dict()
        # sell orders: price -> (quantity, unix time stamp)
        self.asks: Dict[float, Tuple[float, float]] = dict()

        self._bid_prices: ordered_set = ordered_set()
        self._ask_prices: ordered_set = ordered_set()

        self._max_ask_quantity: float = 0
        self._max_bid_quantity: float = 0

    def ask_quantity(self, ask_price: float) -> Optional[float]:
        item = self.asks.get(ask_price)
        if item:
            quantity, _ = item
            return quantity
        return None

    def bid_quantity(self, bid_price: float) -> Optional[float]:
        item = self.bids.get(bid_price)
        if item:
            quantity, _ = item
            return quantity
        return None

    @property
    def product_id(self):
        return self.__product_id

    @property
    def bid_prices(self) -> ordered_set:
        return self._bid_prices

    @property
    def ask_prices(self) -> ordered_set:
        return self._ask_prices

    def update_order(self, otype: OrderType, price: float, quantity: float, time: float):
        if otype == OrderType.BID:
            self.update_bid(price, quantity, time)
        else:
            self.update_ask(price, quantity, time)

    def update_bid(self, price: float, quantity: float, time: float):
        if price in self.bids:
            prev_quantity, prev_time = self.bids[price]
            if time < prev_time:
                return

            if quantity <= 0:
                self.bids.pop(price)
                self._bid_prices.remove(price)
                return

            self.bids[price] = (quantity, time)
            self._max_bid_quantity = max(quantity, self._max_bid_quantity)
        elif quantity > 0:
            self.bids[price] = (quantity, time)
            self._bid_prices.add(price)
            self._max_bid_quantity = max(quantity, self._max_bid_quantity)

    def update_ask(self, price: float, quantity: float, time: float):
        if price in self.asks:
            prev_quantity, prev_time = self.asks[price]
            if time < prev_time:
                return

            if quantity <= 0:
                self.asks.pop(price)
                self._ask_prices.remove(price)
                return

            self.asks[price] = (quantity, time)
            self._max_bid_quantity = max(quantity, self._max_bid_quantity)
        elif quantity > 0:
            self.asks[price] = (quantity, time)
            self._ask_prices.add(price)
            self._max_ask_quantity = max(quantity, self._max_ask_quantity)
