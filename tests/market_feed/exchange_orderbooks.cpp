#include "config.h"
// disble verbose logging
#undef WEBSOCKET_LOGS
#undef MESSAGE_PAYLOAD_LOG

#include "exchange_api.h"
#include "coinbase_feed.h"
#include "binance_feed.h"
#include <iostream>


/**
 * Test program that prints some local order book statistics every time
 * it is updated a couple of seconds then quits.
 */
int main(void) {
    instrument_pair_t ethusd {instrument("ETH"), instrument("USD")};
    instrument_pair_t btcusd {instrument("BTC"), instrument("USD")};
    std::vector<instrument_pair_t> pairs;
    pairs.emplace_back(instrument("ETH"), instrument("USD"));

    market_feed<coinbase_api> cb_feed (pairs, "OVvF5YREDjPdLz9J", "gxDeuHUXte1vbuVRdRy3dHLhXgO0M6ej");
    cb_feed.register_event_handler(feed_event_t(ethusd, feed_event_t::ORDERS_UPDATED), 
            [](const orderbook_t& orderbook) -> bool {
                // NOTE: potentially invalid memory access here, check size of bid/ask maps first
                orderbook_t::order_t max_bid {*orderbook.bid_iterator()};
                orderbook_t::order_t min_ask {*orderbook.ask_iterator()};

                log("[{}] bid: {:.5f}@{:.5f}, ask {:.5f}@{:.5f}, bid/ask: {:.5f}\r",
                        instrument_pair::to_coinbase(orderbook.pair),
                        max_bid.second, max_bid.first,
                        min_ask.second, min_ask.first,
                        (min_ask.first - max_bid.first));

                return true;
            }
        );

    market_feed<binance_api> bi_feed (pairs, "bD9QfIu4FBdRJpviWI075M6KMX2lb9oUyLfC2IknlE4vcIbnFKQaeSm8f0vLW8te", "AfqGK6Jf8HQGiI93RC7jYDJMKVS9cMlc4adhvcXeMSOSUKQEIkmIV9SmeZDu0kd5");
    bi_feed.register_event_handler(feed_event_t(ethusd, feed_event_t::ORDERS_UPDATED), 
            [](const orderbook_t& orderbook) -> bool {
                // NOTE: potentially invalid memory access here, check size of bid/ask maps first
                orderbook_t::order_t max_bid {*orderbook.bid_iterator()};
                orderbook_t::order_t min_ask {*orderbook.ask_iterator()};

                log("[{}] bid: {:.5f}@{:.5f}, ask {:.5f}@{:.5f}, bid/ask: {:.5f}\r",
                        instrument_pair::to_binance(orderbook.pair),
                        max_bid.second, max_bid.first,
                        min_ask.second, min_ask.first,
                        (min_ask.first - max_bid.first));

                return true;
            }
        );

    bi_feed.start_feed();
    cb_feed.start_feed();

    // wait for 10 seconds
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(10s);

    bi_feed.close();
    cb_feed.close();

    bi_feed.join();
    cb_feed.join();
}
