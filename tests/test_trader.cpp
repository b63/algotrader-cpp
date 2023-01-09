#include "config.h"
// disble verbose logging
#undef WEBSOCKET_LOGS
#undef MESSAGE_PAYLOAD_LOG

#include "exchange_api.h"
#include "coinbase_feed.h"
#include "binance_feed.h"
#include <iostream>


class TestTrader 
{
    typedef std::unordered_map<exchange_api_t, std::pair<std::vector<orderbook_t::order_t>,std::vector<orderbook_t::order_t>>>
        map_t;
    map_t m_guarded_book;


public:
    TestTrader(instrument_pair_t product_pair)
    {}


    void find_max_profit_ask(exchange_api_t source_id, orderbook_t::order_t source_bid,
            exchange_api_t target_id, const std::vector<orderbook_t::order_t>& target_asks)
    {
        double max_profit = 0;
        std::vector<orderbook_t::order_t>::const_iterator target_order;
        for (auto target_it = target_asks.begin(); target_it != target_asks.end(); ++target_it)
        {
            auto [target_price, target_quantity] = *target_it;
            if (target_price > source_bid.first )
                break;
            const double diff = source_bid.first - target_price;
            const double min_quantity = std::min(source_bid.second, target_quantity);
            const double profit = min_quantity * diff;
            // log("[{} -> {}] {:.1e} @ {:.1e} -> {:.1e} @ {:.1e}; {:f}",
            //         exchange_api::to_string(source_id),
            //         exchange_api::to_string(target_id),
            //         target_price, target_quantity,
            //         source_bid.first, source_bid.second,
            //         profit);
            if (profit > max_profit)
            {
                target_order = target_it;
                max_profit = profit;
            }

        }

        if (max_profit > 0)
        {
            log("Maximum profit ask: $ {:f}, ({} -> {}) {:f} @ {:.5e} -> {:f} @ {:.5e}",
                    max_profit,
                    exchange_api::to_string(target_id),
                    exchange_api::to_string(source_id),
                    target_order->first, target_order->second,
                    source_bid.first, source_bid.second);
        }
    }

    void find_max_profit_bid(exchange_api_t source_id, orderbook_t::order_t source_ask,
            exchange_api_t target_id, const std::vector<orderbook_t::order_t>& bids)
    {
        double max_profit = 0;
        std::vector<orderbook_t::order_t>::const_iterator target_order;
        for (auto target_it = bids.cbegin(); target_it != bids.cend(); ++target_it)
        {
            auto [target_price, target_quantity] = *target_it;
            if (source_ask.first > target_price)
                break;
            const double diff = target_price - source_ask.first;
            const double min_quantity = std::min(source_ask.second, target_quantity);
            const double profit = min_quantity * diff;
            // log("[{} -> {}] {:.1e} @ {:.1e} -> {:.1e} @ {:.1e}; {:f}",
            //         exchange_api::to_string(source_id),
            //         exchange_api::to_string(target_id),
            //         source_ask.first, source_ask.second,
            //         target_price, target_quantity,
            //         profit);
            if (profit > max_profit)
            {
                target_order = target_it;
                max_profit = profit;
            }

        }
        // NOTE: need to think about how to about duplicates

        if (max_profit > 0)
        {
            log("Maximum profit bid: $ {:f}, ({} -> {}) {:f} @ {:.5e} -> {:f} @ {:.5e}",
                    max_profit,
                    exchange_api::to_string(source_id),
                    exchange_api::to_string(target_id),
                    source_ask.first, source_ask.second,
                    target_order->first, target_order->second);
        }

    }

    bool feed_event_handler(const orderbook_t& book)
    {
        // log("feed_event_handler {} {}", exchange_api::to_string(book.exchange),
        //     instrument_pair::to_coinbase(book.pair));
        // copy over updated guarded bids/asks from orderbook 
        auto& [source_bids, source_asks] = m_guarded_book[book.exchange];
        book.copy_guarded_bids(source_bids);
        book.copy_guarded_asks(source_asks);

        // go through all exchanges
        for(map_t::iterator map_it = m_guarded_book.begin(); map_it != m_guarded_book.end(); ++map_it)
        {
            if (map_it->first == book.exchange)
                continue;
            auto& [target_bids, target_asks] = map_it->second;

            // selling to book.exchange exchange and buying from other exchange
            if (source_bids.size() > 0)
                find_max_profit_ask(book.exchange, source_bids[0], map_it->first, target_asks);

            // buying from book.exchange exchange and selling to other exchange
            if (source_asks.size() > 0)
                find_max_profit_bid(book.exchange, source_asks[0], map_it->first, target_bids);
        }

        return true;
    }

};

/**
 * test program to print out profitables arbritrage trades between exchanges for 10 seconds then quits.
 * format:
 *     Maximum profit <bid|ask>: <max profit> (<exchange A> -> <exchange B>) <price> @ <quantity> -> <price> @ <quantity>
 *
 *  To execute on the arbritrage trade, you'd match the ask order on the exchange A (by submitting a bid order)
 *  and match the bid order on exchange B by submitting a sell order.
 */
int main(void) {
    instrument_pair_t t_pair {instrument("ETH"), instrument("USD")};
    std::vector<instrument_pair_t> pairs;
    pairs.emplace_back(t_pair);

    TestTrader trader(t_pair);

    market_feed<coinbase_api> cb_feed (pairs, "OVvF5YREDjPdLz9J", "gxDeuHUXte1vbuVRdRy3dHLhXgO0M6ej");
    cb_feed.register_event_handler(feed_event_t(t_pair, feed_event_t::ORDERS_UPDATED),
            std::bind(&TestTrader::feed_event_handler, &trader, std::placeholders::_1));

    market_feed<binance_api> bi_feed (pairs, "bD9QfIu4FBdRJpviWI075M6KMX2lb9oUyLfC2IknlE4vcIbnFKQaeSm8f0vLW8te", "AfqGK6Jf8HQGiI93RC7jYDJMKVS9cMlc4adhvcXeMSOSUKQEIkmIV9SmeZDu0kd5");
    bi_feed.register_event_handler(feed_event_t(t_pair, feed_event_t::ORDERS_UPDATED),
            std::bind(&TestTrader::feed_event_handler, &trader, std::placeholders::_1));

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
