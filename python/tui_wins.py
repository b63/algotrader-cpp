from typing import Optional, Tuple, Iterator

import curses

from util import tuilogger, logcall
from order_book import OrderBook


class TUIWin:
    def __init__(self, min_height: int = -1, min_width: int = -1, border_top: int = 1,
                 border_left: int = 1, border_right: int = 1, border_bottom: int = 1) -> None:
        self._height = min_height
        self._width = min_width
        self._min_height = min_height
        self._min_width = min_width

        self._border_top = border_top
        self._border_bottom = border_bottom
        self._border_left = border_left
        self._border_right = border_right

        self._min_x: int = 0
        self._max_x: int = 0
        self._range_x: int = 0

        self._min_y: int = 0
        self._max_y: int = 0
        self._range_y: int = 0

        self.win: Optional[curses.window] = None

    def init_win(self, win: curses.window):
        height, width = win.getmaxyx()
        self.win = win

        self._height = max(self._min_height, height)
        self._width = max(self._min_width, width)

        self._min_y = self._border_top
        self._min_x = self._border_left
        self._max_x = self._width - self._border_right
        self._max_y = self._height - self._border_bottom
        self._range_x = self._max_x - self._min_x
        self._range_y = self._max_y - self._min_y

    def addstr(self, y: int, x: int, text: str, attr: int = curses.A_NORMAL, max_len: int = -1):
        assert self.win

        if max_len > 0:
            if len(text) > max_len > 3:
                text = text[:max_len - 3] + "..."
            else:
                text = text[:max_len]

        try:
            self.win.addstr(y, x, text, attr)
        except curses.error:
            pass

    def addch(self, y: int, x: int, ch: str, attr: int = curses.A_NORMAL):
        assert self.win

        try:
            self.win.addch(y, x, ch, attr)
        except curses.error:
            pass

    def chgat(self, y: int, x: int, num: int, attr: int = curses.A_NORMAL):
        assert self.win

        try:
            self.win.chgat(y, x, num, attr)
        except curses.error:
            pass


class TUIMarketFeedWin(TUIWin):
    def __init__(self, orderbook: OrderBook, stdscr: curses.window, order_height=10, offset_y=0,
                 header: str = "Market Feed",
                 show_order_book=True,
                 bin_size: float = 0.02) -> None:
        super().__init__(8, 30)

        self.stdscr = stdscr
        self.order_win: Optional[TUIOrderWin] = None
        self.summary_win: Optional[TUISummaryWin] = None
        self.orderbook = orderbook

        self.header: str = header

        self._summary_height = 6
        self._show_order_book = show_order_book
        self._order_height = order_height if show_order_book else 0

        self._height = self._order_height + self._summary_height + 2
        self._offset_y = offset_y

        self._prev_term_width = 0

        self._default_bin_size = bin_size
        self._bold_header = False

    def init_win(self):
        tuilogger.info(
            f"TUIMarketFeedWin<{self.header}> init_win before super(): _height {self._height},  _offset_y {self._offset_y}")
        super().init_win(curses.newwin(self._height, curses.COLS, self._offset_y, 0))
        assert self.win
        tuilogger.info(
            f"TUIMarketFeedWin<{self.header}> initialized window with {self.win.getmaxyx()}+({self._offset_y},0)")
        tuilogger.info(f"TUIMarketFeedWin<{self.header}> _height {self._height}, _width {self._width}")
        tuilogger.info(f"TUIMarketFeedWin<{self.header}> _min_height {self._min_height}, _min_width {self._min_width}")

        self.prev_term_width = 0  # force resize upon first refresh

        self.order_win = TUIOrderWin(self.orderbook, bin_size=self._default_bin_size)
        self.summary_win = TUISummaryWin(self.orderbook)

    async def refresh_async(self):
        if not self.win or not self.order_win or not self.summary_win:
            return

        if self._prev_term_width != curses.COLS:
            self.win.resize(self._height, curses.COLS)
            self._prev_term_width = curses.COLS
            self.win.clear()
            tuilogger.info(f"TUIMarketFeedWin<{self.header}> resized to {self.win.getmaxyx()}")

            if self._show_order_book:
                order_win = self.win.derwin(self._order_height, self._range_x, self._min_y, self._min_x)
                self.order_win.init_win(win=order_win)
                tuilogger.info(f"TUIMarketFeedWin<{self.header}> order_win resized to {order_win.getmaxyx()}")

            summary_win = logcall(self.win.derwin, log=tuilogger)(self._summary_height, self._range_x,
                                                                  self._order_height + self._min_y, self._min_x)
            self.summary_win.init_win(win=summary_win)
            tuilogger.info(f"TUIMarketFeedWin<{self.header}> summary_win resized to {summary_win.getmaxyx()}")

        if self._show_order_book:
            self.order_win.reprint()
        self.summary_win.reprint()
        self.win.box()

        # draw header
        header_text = f" {self.header} "
        start_header_x = max(self._min_x, (self._width // 2) - (len(header_text) // 2))
        self.addstr(0, start_header_x, header_text,
                    curses.A_BOLD | curses.A_REVERSE if self._bold_header else curses.A_NORMAL)

        self.win.move(0, 0)
        self.win.noutrefresh()
        # self.stdscr.addch(0,0, "┌")
        curses.doupdate()

    def toggle_bold_header(self):
        self._bold_header = not self._bold_header

    def toggle_track_bid_bottom(self):
        if not self._show_order_book:
            return

        assert self.order_win and self.order_win.win

        self.order_win.track_bid_bottom = not self.order_win.track_bid_bottom

    async def scroll_orders(self, lines: int = 0, reset=False):
        if not self._show_order_book:
            return

        assert self.order_win and self.order_win.win
        self.order_win.scroll_orders(lines=lines, reset=reset)

        self.order_win.reprint()
        await self.refresh_async()


class TUISummaryWin(TUIWin):
    def __init__(self, orderbook: OrderBook) -> None:
        super().__init__(6, 30)
        self.orderbook = orderbook
        self.header = "Summary"

    def init_win(self, win: curses.window):
        super().init_win(win)
        tuilogger.debug(f"TUISummaryWin<{self.orderbook.name}> init_win {win.getmaxyx()}")

    def reprint(self):
        assert self.win

        bid_prices = self.orderbook.bid_prices
        ask_prices = self.orderbook.ask_prices

        min_bid_price, max_bid_price = 0, 0
        max_ask_price, min_ask_price = 0, 0

        if len(bid_prices) > 0:
            min_bid_price, max_bid_price = bid_prices[0], bid_prices[-1]

        if len(ask_prices) > 0:
            min_ask_price, max_ask_price = ask_prices[0], ask_prices[-1]

        y = self._min_y
        self.addstr(y, self._min_x,
                    f"Max. Bid: {max_bid_price:6,.2f} ({self.orderbook.bid_quantity(max_bid_price):.2e})")
        self.addstr(y + 1, self._min_x,
                    f"Min. Ask: {min_ask_price:6,.2f} ({self.orderbook.ask_quantity(min_ask_price):.2e})")

        self.addstr(y + 3, self._min_x,
                    f"Min. Bid: {min_bid_price:6,.2f} ({self.orderbook.bid_quantity(min_bid_price):.2e})")
        self.addstr(y + 4, self._min_x,
                    f"Max. Ask: {max_ask_price:6,.2f} ({self.orderbook.ask_quantity(max_ask_price):.2e})")

        second_start_x = self._width // 2 + 1
        spread = min_ask_price - max_bid_price
        self.addstr(y, second_start_x, f"Bid/Ask: {spread:6,.4f}")
        self.addstr(y + 2, second_start_x, f"Total Bid: {len(bid_prices):6,d}")
        self.addstr(y + 3, second_start_x, f"Total Ask: {len(ask_prices):6,d}")

        for i in range(self._width):
            self.win.addch(0, i, '─', curses.A_NORMAL)

        # draw header
        header_text = f" {self.header} "
        start_header_x = max(self._min_x, (self._width // 2) - (len(header_text) // 2))
        self.addstr(0, start_header_x, header_text)


class TUIOrderWin(TUIWin):
    def __init__(self, orderbook: OrderBook, bin_size: float = 0.02, price_start: float = 0.0) -> None:
        # min height, min width
        super().__init__(6, 30)

        self.orderbook: OrderBook = orderbook

        self.bin_size: float = bin_size
        self.price_start: float = price_start

        self._orders_col_width: int = 0
        self._price_col_width: int = 0
        self._quantity_dot_offset: int = -1

        self.track_bid_bottom: bool = True

    def init_win(self, win: curses.window):
        super().init_win(win)
        tuilogger.debug(f"TUIOrderWin<{self.orderbook.name}> init_win {win.getmaxyx()}")

        self._orders_col_width = self._width // 3
        self._price_col_width = self._width - 2 * (self._width // 3)

    def scroll_orders(self, lines: int = 0, reset=False):
        if reset:
            if len(self.orderbook.bid_prices) > 0:
                reset_price = self.bin_size * (self.orderbook.bid_prices[-1] // self.bin_size) + 2 * self.bin_size
            else:
                reset_price = 0
            self.price_start = reset_price
        else:
            if lines != 0:
                self.track_bid_bottom = False
            self.price_start += lines * self.bin_size

    def _print_prices(self):

        if len(self.orderbook.bid_prices) > 0:
            highlight_price_bid = self.orderbook.bid_prices[-1]
        else:
            highlight_price_bid = None

        if len(self.orderbook.ask_prices) > 0:
            highlight_price_ask = self.orderbook.ask_prices[0]
        else:
            highlight_price_ask = None

        price = self.price_start + self.bin_size
        for y in range(self._min_y, self._max_y + 1):
            range_start, range_end = price - self.bin_size, price

            price_text = self.align_dot(f"{range_start:.2f}", self._price_col_width, offset=0)
            x = max(self._min_x, self._width // 2 - len(price_text) // 2)
            self.addstr(y, x, price_text, curses.A_NORMAL)
            ind_digit_front = self._index_neg(price_text, " ")
            ind_digit_back = self._index_neg(price_text, " ", reverse=True)

            if highlight_price_ask and range_start <= highlight_price_ask < range_end:
                self.chgat(y, x + ind_digit_front - 1, self._orders_col_width + self._price_col_width - ind_digit_front,
                           curses.A_REVERSE)

            if highlight_price_bid and range_start <= highlight_price_bid < range_end:
                self.chgat(y, self._min_x, self._orders_col_width + ind_digit_back + 1, curses.A_REVERSE)

            price -= self.bin_size

    def _index_neg(self, text: str, char: str, reverse: bool = False) -> int:
        r = range(len(text) - 1, -1, -1) if reverse else range(len(text))
        for i in r:
            if text[i] != char[0]:
                return i
        return -1

    @staticmethod
    def align_dot(text: str, width: int = 10, offset: int = 0, fill_char=" ") -> str:
        center = width // 2
        assert center > abs(offset)

        index = text.index(".")
        before, after = text[:index], text[index + 1:]

        rem_width = center + offset
        if len(before) > rem_width:
            return text.center(width, fill_char)

        before_padded = before.rjust(rem_width, fill_char)
        after_padded = after.ljust(width - center - offset - 1, fill_char)
        return f"{before_padded}.{after_padded}"

    def get_order_rows(self, num_rows: int, bid_orders=True) -> Iterator[Tuple[float, float, float]]:
        if bid_orders:
            prices_arr = self.orderbook.bid_prices
            max_quantity = self.orderbook._max_bid_quantity
            quanitity_func = self.orderbook.bid_quantity
        else:
            prices_arr = self.orderbook.ask_prices
            max_quantity = self.orderbook._max_ask_quantity
            quanitity_func = self.orderbook.ask_quantity

        if max_quantity == 0:
            max_quantity = 1

        n = len(prices_arr)

        bin_count = 0
        price = self.price_start + self.bin_size
        ind = prices_arr.indexof(price)

        for _ in range(num_rows):
            # range of bin is [price_start, price)
            price_start = price - self.bin_size

            # count number of bin orders within range of bin
            quantity = 0
            bin_count = 0
            while 0 <= ind < n and prices_arr[ind] > price_start:
                _quantity = quanitity_func(prices_arr[ind])
                if _quantity:
                    quantity += _quantity
                bin_count += 1
                ind -= 1
            price = price_start

            yield price_start, quantity, max(0, min(1, quantity / max_quantity))

    def _print_order_row(self, y: int, x: int, order: Tuple[float, float, float]):
        assert self.win

        _, quantity, _ = order
        if quantity >= 10 ** 6 or quantity <= 0.001:
            quantity_text = f"{quantity:,.3e}"
        elif quantity == 0:
            quantity_text = ""
        else:
            quantity_text = f"{quantity:,.3f}"

        if quantity != 0:
            quantity_text = self.align_dot(quantity_text, self._orders_col_width, offset=self._quantity_dot_offset)
        else:
            quantity_text = " " * self._orders_col_width
        self.addstr(y, x, quantity_text)

    def _print_headers(self, attr: int = curses.A_BOLD):
        header = " Bids "
        self.addstr(self._min_y - 1, self._orders_col_width // 2 - len(header) // 2, header, attr)

        header = " Prices "
        self.addstr(self._min_y - 1, self._width // 2 - len(header) // 2, header, attr)

        header = " Asks "
        self.addstr(self._min_y - 1, self._width - self._orders_col_width // 2 - len(header) // 2, header, attr)

    def reprint(self):
        assert self.win

        if self.track_bid_bottom:
            self.price_start = self.bin_size * (self.orderbook.bid_prices[-1] // self.bin_size) + 2 * self.bin_size
            tuilogger.info(f"TUIOrderWin tracking mode on, bin start: {self.price_start}")

        tuilogger.info(f"(refresh) TUIWin {self.win.getmaxyx()}")
        tuilogger.info(f"(refresh) TUIWin price start: {self.price_start}, bin size: {self.bin_size}")

        # draw the number of bid orders that fit
        y, x = self._min_y, self._min_x
        for order in self.get_order_rows(self._max_y - self._min_y, bid_orders=True):
            if order is None:
                # print empty line
                self.addstr(y, x, " " * self._orders_col_width)
                y += 1
                continue

            self._print_order_row(y, x, order)
            y += 1

        # draw the number of ask orders that fit
        y, x = self._min_y, self._max_x - self._orders_col_width
        for order in self.get_order_rows(self._max_y - self._min_y, bid_orders=False):
            if order is None:
                # print empty line
                self.addstr(y, x, " " * self._orders_col_width)
                y += 1
                continue

            self._print_order_row(y, x, order)
            y += 1

        self._print_prices()

        self.win.box()
        self._print_headers()
