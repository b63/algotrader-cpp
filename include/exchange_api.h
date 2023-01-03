#ifndef _EXCHANGE_API_H
#define _EXCHANGE_API_H

#include "json.h"

#include <cstring>

#include <string>
#include <unordered_map>
#include <functional>
#include <any>

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
        return reinterpret_cast<uint64_t>(lhs._buf) == reinterpret_cast<uint64_t>(rhs._buf);
    }

    private:
        uint8_t  _buf[instrument::BUF_BYTES];
};


typedef std::pair<instrument, instrument> instrument_pair_t;
namespace instrument_pair {
    std::string to_coinbase(instrument_pair_t pair);
    std::string to_binance(instrument_pair_t pair);
    std::string to_binance_lower(instrument_pair_t pair);
}



enum class exchange_apis {
    COINBASE_ADVANCED,
    BINANCE,
    WEBULL
};

struct coinbase_api {
    static constexpr const exchange_apis exchange_api_id = exchange_apis::COINBASE_ADVANCED;
    static constexpr const char* SOCKET_URI = "wss://advanced-trade-ws.coinbase.com";
};

struct binance_api {
    static constexpr const exchange_apis exchange_api_id = exchange_apis::BINANCE;
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

    const exchange_apis exchange;
    const instrument_pair_t pair;

    orderbook_t(instrument_pair_t pair, exchange_apis exchange_id)
        : exchange{exchange_id}, pair {pair}, m_map{}
    {

    }

    template <typename T, typename... Args>
    void process_order_updates(Args&&...) requires is_exchange_api<T>;

    template <typename T, typename... Args>
    void process_order_snapshot(Args&&...) requires is_exchange_api<T>;

    template <typename T, typename... Args>
    void process_ticker_update(Args&&...) requires is_exchange_api<T>;


    template <>
    void process_order_updates<coinbase_api>(const Value& updates)
    { }

    template <>
    void process_order_updates<binance_api>(const Value& bids, const Value& asks)
    { }

    template <>
    void process_order_snapshot<coinbase_api>(const Value& updates)
    { }

    template <>
    void process_order_snapshot<binance_api>(const Value& bids, const Value& asks)
    { }

    template <>
    void process_ticker_update<coinbase_api>(const Value& updates)
    { }

    template <>
    void process_ticker_update<binance_api>(const Value& update)
    { }

private:
    std::unordered_map<key_t, value_t> m_map;

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
