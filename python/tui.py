from order_book import OrderBook
from tui_wins import TUIMarketFeedWin

import asyncio
import curses

from typing import List, Dict
from util import tuilogger
from functools import partial


async def wrapper(func, /, *args, **kwds):
    """Wrapper function that initializes curses and calls another function,
    restoring normal keyboard/screen behavior on error.
    The callable object 'func' is then passed the main window 'stdscr'
    as its first argument, followed by any other arguments passed to
    wrapper().
    """

    stdscr = None
    try:
        # Initialize curses
        stdscr = curses.initscr()

        # Turn off echoing of keys, and enter break mode,
        # where no buffering is performed on keyboard input
        curses.noecho()
        curses.cbreak()

        # In keypad mode, escape sequences for special keys
        # (like the cursor keys) will be interpreted and
        # a special value like curses.KEY_LEFT will be returned
        stdscr.keypad(True)

        # Start color, too.  Harmless if the terminal doesn't have
        # color; user can test with has_color() later on.  The try/catch
        # works around a minor bit of over-conscientiousness in the curses
        # module -- the error return from C start_color() is ignorable.
        try:
            curses.start_color()
        except:
            pass

        await func(stdscr, *args, **kwds)
    finally:
        # Set everything back to normal
        if stdscr:
            stdscr.keypad(False)
            curses.echo()
            curses.nocbreak()
            curses.endwin()


class TUI:
    def __init__(self, orderbooks: List[OrderBook], feed_tasks: List[asyncio.Task],
                 only_summary=False, bin_size: float = 0.2) -> None:
        self.feed_tasks = feed_tasks

        self.disabled = True
        self.orderbooks = orderbooks
        self.only_summary = only_summary
        self._default_bin_size = bin_size

        self.feedtuis: Dict[OrderBook, TUIMarketFeedWin] = dict()
        self._refresh_tasks = set()

    async def cancel_feed_tasks(self):
        if not self.feed_tasks:
            return
        for i, task in enumerate(self.feed_tasks):
            print(f"Canceling task {i + 1}/{len(self.feed_tasks)} ...")
            task.cancel()
            try:
                await task
            except asyncio.CancelledError:
                pass

    def market_feed_updated(self, orderbook: OrderBook):
        if orderbook not in self.feedtuis:
            return

        feedtui = self.feedtuis[orderbook]

        def task_done_callback(self, task):
            if self and self._refresh_tasks:
                self._refresh_tasks.discard(task)

            e = task.exception()
            if type(e) is curses.error:
                tuilogger.error("Refresh task raised curses.error, ignoring", exc_info=e)
            elif e:
                tuilogger.error("Refresh task raised exception", exc_info=e)
                tuilogger.info("disabling TUI ...")
                self.disabled = True

        refresh_task = asyncio.create_task(feedtui.refresh_async())
        self._refresh_tasks.add(refresh_task)
        refresh_task.add_done_callback(partial(task_done_callback, self))

    async def tui(self, stdscr: curses.window):
        self.disabled = False
        self.stdscr = stdscr

        stdscr.clear()
        stdscr.nodelay(True)

        offset_y = 0
        for orderbook in self.orderbooks:
            feedtui = TUIMarketFeedWin(orderbook, order_height=8, offset_y=offset_y, header=f"{orderbook.name}",
                                       show_order_book=not self.only_summary,
                                       bin_size=self._default_bin_size, stdscr=stdscr)
            feedtui.init_win()
            offset_y += feedtui._height
            self.feedtuis[orderbook] = feedtui

        # tui user input loop
        feedtui_list = list(self.feedtuis.values())
        selected_index = 0
        feedtui_list[0].toggle_bold_header()

        while True:
            feedtui = feedtui_list[selected_index]
            termy, termx = stdscr.getmaxyx()
            if termy != curses.LINES or termx != curses.COLS:
                # resizing will be handled on next window refresh
                curses.update_lines_cols()

            key = stdscr.getch()
            if self.disabled or key == ord('q'):
                self.disabled = True
                stdscr.keypad(False)
                curses.echo()
                curses.nocbreak()
                curses.endwin()
                print(f"quitting ...")
                break

            if key == ord('t'):
                feedtui.toggle_track_bid_bottom()
                await feedtui.refresh_async()
            elif key == ord('j'):
                await feedtui.scroll_orders(-1)
            elif key == ord('k'):
                await feedtui.scroll_orders(1)
            elif key == ord('g'):
                await feedtui.scroll_orders(reset=True)
            elif key == ord('n'):
                selected_index = (selected_index + 1) % len(feedtui_list)
                next_feedtui = feedtui_list[selected_index]
                feedtui.toggle_bold_header()
                next_feedtui.toggle_bold_header()
                await feedtui.refresh_async()
                await next_feedtui.refresh_async()
            else:
                await asyncio.sleep(0.1)
                continue
            # refresh quicker if key pressed
            await asyncio.sleep(0.01)

        await self.cancel_feed_tasks()
