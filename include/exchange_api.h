#ifndef _EXCHANGE_API_H
#define _EXCHANGE_API_H

#include "json.h"
#include "logger.h"

#include <cstring>

#include <string>
#include <map>
#include <queue>
#include <vector>
#include <functional>
#include <any>
#include <chrono>

template<typename T, typename... U>
concept is_any = (std::same_as<T, U> || ...);

struct instrument {
    static constexpr const size_t BUF_BYTES = 8;
    instrument (const std::string& code)
        : _buf{}
    {
        for (int i = 0; i < instrument::BUF_BYTES; ++i)
        {
            _buf[i] = i < code.length() ? std::toupper(code[i]) : 0;
        }
    }

    std::string name()
    {
        const char *ch = reinterpret_cast<const char*>(&_buf[0]);
        const char* null = reinterpret_cast<const char*>(std::memchr(ch, '\0', BUF_BYTES));

        std::string s{ch, static_cast<size_t>(null-ch)};
        return s;
    }

    std::string name_lower()
    {
        std::string name_upper {name()};
        for (char& c : name_upper)
            c = std::tolower(c);

        return name_upper;
    }

    friend bool operator==(const instrument& lhs, const instrument& rhs)
    {
        return *reinterpret_cast<const uint64_t*>(&lhs._buf) == *reinterpret_cast<const uint64_t*>(&rhs._buf);
    }

    private:
        uint8_t  _buf[instrument::BUF_BYTES];
};


typedef std::pair<instrument, instrument> instrument_pair_t;
namespace instrument_pair {
    std::string to_coinbase(instrument_pair_t pair);
    std::string to_binance(instrument_pair_t pair);
    std::string to_binance_lower(instrument_pair_t pair);

    inline bool same(const instrument_pair_t& lhs, const instrument_pair_t& rhs)
    {
        return lhs.first == rhs.first && lhs.second == rhs.second;
    }
}

inline bool operator==(const instrument_pair_t& lhs, const instrument_pair_t& rhs)
{
    return lhs.first == rhs.first && lhs.second == rhs.second;
}



enum class exchange_api_t : int {
    COINBASE_ADVANCED,
    BINANCE,
    WEBULL
};

namespace exchange_api {
    constexpr const char* to_string(exchange_api_t id)
    {
        if (id == exchange_api_t::COINBASE_ADVANCED)
            return "Coinbase";
        if (id == exchange_api_t::BINANCE)
            return "Binance";
        return "WeBull";
    }
};

struct coinbase_api {
    static constexpr const exchange_api_t exchange_api_id = exchange_api_t::COINBASE_ADVANCED;
    static constexpr const char* SOCKET_URI = "wss://advanced-trade-ws.coinbase.com";
};

struct binance_api {
    static constexpr const exchange_api_t exchange_api_id = exchange_api_t::BINANCE;
    static constexpr const char* SOCKET_URI = "wss://stream.binance.us:9443";
    static constexpr const char* SNAPSHOT_URL = "https://www.binance.us/api/v1/depth";
};

template <typename ExchangeAPI>
concept is_exchange_api = is_any<ExchangeAPI, coinbase_api, binance_api>;

template <typename ExchangeApi>
    requires is_exchange_api<ExchangeApi>
class market_feed {};


struct orderbook_t {
    typedef double key_t;
    typedef double value_t;
    typedef std::map<key_t, value_t> map_t;
    typedef std::pair<key_t, value_t> order_t;

    static constexpr const size_t GUARDED_SUBSET_SIZE = 10;
    const exchange_api_t exchange;
    const instrument_pair_t pair;

    struct ticker_t
    {
        typedef std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds> time_point;
        // wrapper type to encapsulate movement of market from time point `start` to `end`
        instrument_pair_t pair;
        double            price;   // average price
        double            low;     // lowest price during the interval
        double            high;    // highest price during the interval
        double            opening; // opening price for the interval
        double            closing; // closing price for the interval
        double            volume;  // total volume for the interval
        time_point        start;   // starting time point for the interval
        time_point        end;     // ending time poing for the interval
    };

    orderbook_t(instrument_pair_t pair, exchange_api_t exchange_id)
        : exchange{exchange_id}, 
          pair {pair}, m_bid_map{}, m_ask_map{},
          m_guarded_bids{}, m_guarded_asks{}
    {
        m_guarded_bids.reserve(GUARDED_SUBSET_SIZE);
        m_guarded_asks.reserve(GUARDED_SUBSET_SIZE);
    }

    template <typename T, typename... Args>
    void process_order_updates(Args&&...) requires is_exchange_api<T>;

    template <typename T, typename... Args>
    void process_order_snapshot(Args&&...) requires is_exchange_api<T>;

    template <typename T, typename... Args>
    void process_ticker_update(Args&&...) requires is_exchange_api<T>;


    template <>
    void process_order_updates<coinbase_api>(const Value& updates)
    { 
        if (!updates.IsArray())
            return;

        for (size_t i = 0; i < updates.Size(); ++i)
        {
            const Value& update   = updates[i];
            const Value& side     = update["side"];
            const Value& price    = update["price_level"];
            const Value& quantity = update["new_quantity"];

            const double price_d    = std::stold(price.GetString());
            const double quantity_d = std::stold(quantity.GetString());

            if (!std::strncmp("bid", side.GetString(), side.GetStringLength()))
                update_bid(price_d, quantity_d);
            else if (!std::strncmp("offer", side.GetString(), side.GetStringLength()))
                update_ask(price_d, quantity_d);
        }

        update_guarded_bids();
        update_guarded_asks();
    }

    template <>
    void process_order_updates<binance_api>(const Value& bids, const Value& asks)
    {
        if (bids.IsArray())
        {
            for (size_t i = 0; i < bids.Size(); ++i)
            {
                const Value& bid = bids[i];
                if (!bid.IsArray()) continue;
                const Value& price    = bid[0];
                const Value& quantity = bid[1];

                const double price_d    = std::stold(price.GetString());
                const double quantity_d = std::stold(quantity.GetString());
                update_bid(price_d, quantity_d);
            }
            update_guarded_bids();
        }

        if (asks.IsArray())
        {
            for (size_t i = 0; i < asks.Size(); ++i)
            {
                const Value& ask = asks[i];
                if (!ask.IsArray()) continue;

                const Value& price    = ask[0];
                const Value& quantity = ask[1];

                const double price_d    = std::stold(price.GetString());
                const double quantity_d = std::stold(quantity.GetString());
                update_ask(price_d, quantity_d);
            }
            update_guarded_asks();
        }

    }

    template <>
    void process_order_snapshot<coinbase_api>(const Value& updates)
    {
        process_order_updates<coinbase_api>(updates);
    }

    template <>
    void process_order_snapshot<binance_api>(const Value& bids, const Value& asks)
    { 
        process_order_updates<binance_api>(bids, asks);
    }

    template <>
    void process_ticker_update<coinbase_api>(const Value& updates)
    {
        // TODO: coinbase doesn't price much informaiton (like opening price/closing price) or
        //       a configurable time window, calculate it ourselves based on local order book?
    }

    template <>
    void process_ticker_update<binance_api>(const Value& update)
    {
        // TODO
    }


    void copy_guarded_bids(std::vector<order_t>& dst) const
    {
        const size_t size = m_guarded_bids.size();
        dst.resize(size);
        if (size == 0)
            return;

        std::lock_guard<std::mutex> lock{m_mutex_bids};

        std::memcpy(&dst[0], &m_guarded_bids[0], sizeof(order_t) * size);
    }

    void copy_guarded_asks(std::vector<order_t>& dst) const
    {
        const size_t size = m_guarded_asks.size();
        dst.resize(size);
        if (size == 0)
            return;

        std::lock_guard<std::mutex> lock{m_mutex_asks};

        std::memcpy(&dst[0], &m_guarded_asks[0], sizeof(order_t) * size);
    }

    map_t::const_iterator ask_iterator() const
    { return m_ask_map.cbegin(); }

    map_t::const_iterator ask_iterator_end() const
    { return m_ask_map.cend(); }

    size_t asks_size() const
    { return m_ask_map.size(); }

    map_t::const_reverse_iterator bid_iterator() const
    { return m_bid_map.crbegin(); }

    map_t::const_reverse_iterator bid_iterator_end() const
    { return m_bid_map.crend(); }

    size_t bids_size() const
    { return m_bid_map.size(); }


private:
    map_t m_bid_map;
    map_t m_ask_map;
    //ticker_t m_ticker;

    std::vector<order_t>  m_guarded_bids;
    std::vector<order_t>  m_guarded_asks;
    mutable std::mutex    m_mutex_bids;
    mutable std::mutex    m_mutex_asks;

    void update_bid(double price, double quantity)
    {
        decltype(m_bid_map)::iterator it {m_bid_map.find(price)};
        if (it == m_bid_map.end())
        {
            // price doesn't exist in map
            if (quantity > 0) m_bid_map.emplace(price, quantity);
            return;
        }

        if (quantity <= 0)
        {
            // remove price
            m_bid_map.erase(price);
            return;
        }
        else
        {
            // update price
            it->second = quantity;
        }
    }

    void update_ask(double price, double quantity)
    {
        decltype(m_ask_map)::iterator it {m_ask_map.find(price)};
        if (it == m_ask_map.end())
        {
            // price doesn't exist in map
            if (quantity > 0) m_ask_map.emplace(price, quantity);
            return;
        }

        if (quantity <= 0)
        {
            // remove price
            m_ask_map.erase(price);
            return;
        }
        else
        {
            // update price
            it->second = quantity;
        }
    }

    void update_guarded_bids()
    {
        std::lock_guard<std::mutex> lock {m_mutex_bids};
        // TODO: consider write to a local vector first then copy to member field with memcpy?
        m_guarded_bids.clear();
        const size_t size = std::min(GUARDED_SUBSET_SIZE, m_bid_map.size());

        auto it  = m_bid_map.crbegin();
        for(size_t i = 0; i < size; ++i, ++it)
        {
            m_guarded_bids.emplace_back(it->first, it->second);
        }
    }

    void update_guarded_asks()
    {
        std::lock_guard<std::mutex> lock {m_mutex_asks};
        m_guarded_asks.clear();
        const size_t size = std::min(GUARDED_SUBSET_SIZE, m_ask_map.size());

        auto it  = m_ask_map.cbegin();
        for(size_t i = 0; i < size; ++i, ++it)
        {
            m_guarded_asks.emplace_back(it->first, it->second);
        }
    }

};


typedef std::function<bool(const orderbook_t&)> feed_event_handler_t;
typedef bool(*feed_event_handler_ptr)(const orderbook_t&, std::any& state);

struct feed_event_t {
    enum event_type : int8_t {
        ORDERS_UPDATED=0x1,
        TICKER_UPDATED=0x2,
        ALL=-1,
    };

    feed_event_t(instrument_pair_t pair, event_type mask)
        : product_pair{pair}, update_mask{mask}
    {}

    instrument_pair_t product_pair;
    event_type update_mask;
};

/**
 * MarketFeed is any type that defines the following member functions:
 *     void start_feed(void)
 *         -> to spawn a thread and start listening for market feed updates
 *
 *     void join(void)
 *         -> to block the current thread until the spawned thread finishes
 *
 *     void close(void)
 *         -> signal to the websocket to close (thread will eventually finish soon after)
 *
 *     void register_event_handler(feed_event_t, feed_event_handler_t);
 *         -> the callable passed as second parameter will be invoked *SYNCHRONOUSLY* every
 *            time the provided feed event occurs.
 *            The callable should have the signature:
 *                    bool feed_event_handler(const orderbook&).
 *
 *            The return value signals whether other to stop signaling subsequent event handlers,
 *            and the orderbook is a reference to the orderbook that triggered the event.
 *
 *      void register_raw_event_handler(feed_event_t, raw_feed_event_handler_t, std::any state)
 *          -> same as register_event_handler, but use raw function pointers.
 *             Additional state that is provided when registering the handler is provided
 *             to the handler as a std::any instance.
 */
template <typename MarketFeed>
concept is_market_feed = requires (MarketFeed mf, feed_event_handler_t handler, feed_event_t et,
        feed_event_handler_ptr raw_handler)
{
    mf.start_feed();
    mf.join();
    mf.close();
    mf.register_event_handler(et, handler);
    mf.register_raw_event_handler(et, raw_handler);
};

#endif
