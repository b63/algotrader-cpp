import logging
from typing import List, Tuple

import asyncio
import websockets
import click

from order_book import OrderBook
from market_feed import MarketFeed, CoinBaseMaketFeed, BinanceMaketFeed
from tui import TUI, wrapper
from util import logger, tradelogger


def arbritrage_trade_watcher(target_orderbook: OrderBook, logger_obj: logging.Logger = tradelogger):
    def arbritrage_inner(orderbook: OrderBook):
        if len(orderbook.ask_prices) == 0 or len(target_orderbook.bid_prices) == 0:
            return

        min_ask = orderbook.ask_prices[0]  # buy
        max_bid = target_orderbook.bid_prices[-1]  # sell
        if min_ask >= max_bid:
            return

        diff = max_bid - min_ask
        ask_quantity = orderbook.ask_quantity(min_ask)
        bid_quantity = target_orderbook.bid_quantity(max_bid)
        if not ask_quantity or not bid_quantity:
            return

        max_quantity = min(ask_quantity, bid_quantity)
        max_profit = max_quantity * diff
        if max_profit < 0.001:
            max_profit_text = f"{max_profit:.4e}"
        else:
            max_profit_text = f"{max_profit:.6f}"

        logger_obj.info(
            f"Found trade {orderbook.name} [{ask_quantity:.2E} @ ${min_ask})] -> [{target_orderbook.name} {bid_quantity:.2E} @ ${max_bid}], max profit: ${max_profit_text}")

    return arbritrage_inner


async def monitor_market_feed(orderbook: OrderBook, exchangefeed: MarketFeed, max_messages=-1):
    MAX_MSG_SIZE = 32 * 10 ** 6  # 32 MB
    url = exchangefeed.API_URL

    socketlogger = logging.getLogger("websockets")
    socketlogger.setLevel(logging.DEBUG)

    async with websockets.connect(url, logger=socketlogger, ssl=True, max_size=MAX_MSG_SIZE,
                                  user_agent_header=None) as ws:
        await exchangefeed.subscribe_to_feed(ws)

        count = 0
        async for raw_msg in ws:
            count += 1
            logger.debug(f"<<< ({count}) {raw_msg[:125]}")
            code = exchangefeed.process_message(raw_msg)
            if code < 0:
                await exchangefeed.unsubscribe_to_feed(ws)
                break

            if count == max_messages:
                await exchangefeed.unsubscribe_to_feed(ws)
                await ws.close(1000)


async def main_async(pairs: List[Tuple[OrderBook, MarketFeed]], bin_size: float, no_book: bool = False):
    assert len(pairs) > 0

    market_feed_tasks = []
    for orderbook, exchangefeed in pairs:
        feed_task: asyncio.Task = asyncio.create_task(monitor_market_feed(orderbook, exchangefeed, max_messages=-1))
        market_feed_tasks.append(feed_task)

    tui: TUI = TUI(orderbooks=[orderbook for orderbook, _ in pairs], feed_tasks=market_feed_tasks,
                   only_summary=no_book, bin_size=bin_size)
    for orderbook, exchangefeed in pairs:
        exchangefeed.attach_update_listener(tui.market_feed_updated)

        for target_orderbook, _ in pairs:
            if orderbook == target_orderbook:
                continue
            exchangefeed.attach_update_listener(arbritrage_trade_watcher(target_orderbook))

    tui_task = asyncio.create_task(wrapper(tui.tui))
    try:
        for feed_task in market_feed_tasks:
            await feed_task
        await tui_task
    except asyncio.exceptions.CancelledError:
        # print('== Cancelled task ==')
        pass


@click.command()
@click.option("--coinbase", default="",
              help="name of coinbase product for which to show market feed (eg. ETH-USD) ")
@click.option("--binance", default="",
              help="name of binance symbol for which to show market feed (eg. ETHUSD) ")
@click.option("--no-book", is_flag=True, default=False,
              help="name of binance symbol for which to show market feed (eg. ETHUSD) ")
@click.option("--bin-size", default="0.2",
              help="step size for the price bins in order book table")
def start_event_loop(coinbase, binance, no_book, bin_size):
    bin_size = float(bin_size)

    pairs = []
    if coinbase:
        orderbook = OrderBook(product_id=coinbase, name="Coinbase")
        exchangefeed = CoinBaseMaketFeed(orderbook)
        pairs.append((orderbook, exchangefeed))
    if binance:
        orderbook = OrderBook(product_id=binance, name="Binance")
        exchangefeed = BinanceMaketFeed(orderbook)
        pairs.append((orderbook, exchangefeed))

    if not pairs:
        print("error: provide at least one option, see --help.")
        return

    asyncio.run(main_async(pairs, no_book=no_book, bin_size=bin_size))


if __name__ == "__main__":
    start_event_loop()
