#ifndef _BINANCE_FEED_H
#define _BINANCE_FEED_H

#include "exchange_api.h"
#include "market_socket.h"
#include "crypto.h"
#include "json.h"
#include "requests.h"

#include <thread>
#include <string>
#include <cstring>

template <>
class market_feed<binance_api>
{
public:
    market_feed<binance_api>(const std::vector<instrument_pair_t>& pairs,
            const std::string& api_key, const std::string& secret_key)
        : m_pairs {pairs}, m_streams {"depth@100ms", "kline_1s"},
          m_secret_key {secret_key},
          m_api_key {api_key},
          m_socket{nullptr},
          m_thread {nullptr},
          m_stop_source {},
          m_orderbooks{},
          m_handlers{},
          m_raw_handlers{},
          m_get_snapshot{true}
    {
        for (auto& pair : m_pairs)
        {
            m_orderbooks.emplace(instrument_pair::to_binance(pair), orderbook_t{pair, binance_api::exchange_api_id});
        }
    }

    void start_feed()
    {
        if (m_thread && m_thread->joinable())
            throw std::runtime_error("start_feed called when another is already running in another thread");

        std::stringstream uri {};
        uri << binance_api::SOCKET_URI << "/stream?streams=";
        for (auto i = 0; i < m_streams.size(); ++i)
        {
            const auto& stream = m_streams[i];
            for (auto j = 0; j < m_pairs.size(); ++j)
            {
                uri << instrument_pair::to_binance_lower(m_pairs[j]) << "@" << stream;
                if (j+1 != m_pairs.size() || i+1 != m_streams.size())
                    uri << "/";
            }
        }

        m_socket = std::make_unique<market_feed_socket>(uri.str(), 
                std::bind(&market_feed<binance_api>::message_handler, this, std::placeholders::_1));

        m_thread = std::make_unique<std::jthread>(&market_feed<binance_api>::_start_feed, this, m_stop_source.get_token());
    }

    void join()
    {
        if (m_thread)
            m_thread->join();
    }

    void close()
    {
        if (!m_socket) return;

        m_socket->close();
    }

    void register_event_handler(const feed_event_t& ev, feed_event_handler_t handler)
    {
        m_handlers.emplace_back(ev, handler);
    }

    void register_raw_event_handler(const feed_event_t& ev, feed_event_handler_ptr handler, std::any state)
    {
        m_raw_handlers.emplace_back(ev, handler, std::move(state));
    }


private:
    std::vector<instrument_pair_t>       m_pairs;
    std::array<std::string,2>            m_streams;
    std::string                          m_secret_key;
    std::string                          m_api_key;
    std::unique_ptr<market_feed_socket>  m_socket;
    std::unique_ptr<std::jthread>        m_thread;
    std::stop_source                     m_stop_source;
    std::unordered_map<std::string, orderbook_t> m_orderbooks;

    typedef int snapshot_id_t;
    typedef int update_id_t;
    std::unordered_map<std::string, std::pair<snapshot_id_t, update_id_t>> m_update_ids;

    std::vector<std::tuple<feed_event_t, feed_event_handler_t>>               m_handlers;
    std::vector<std::tuple<feed_event_t, feed_event_handler_ptr, std::any>> m_raw_handlers;
    bool m_get_snapshot;

    void _start_feed(const std::stop_token &stop_token)
    {
        if (!m_socket)
            throw std::runtime_error("_start_feed called in invalid state");
        std::error_code ec;
        m_get_snapshot = true;
        m_socket->connect(ec);
    }

    void process_orderbook_snapsots()
    {
        requests_t req{};
        std::vector<std::string> pair_vec;
        for (auto& [pair_str, orderbook] : m_orderbooks)
        {
            pair_vec.emplace_back(pair_str);
            req.add_request(binance_api::SNAPSHOT_URL, ReqType::GET)
                .add_url_param("symbol", pair_str)
                .add_url_param("limit", "5000")
                .add_header("x-mbx-apikey", m_api_key);
        }

        std::vector<CURLcode> statuses;
        req.fetch_all(statuses);
        for(size_t i = 0; i < pair_vec.size(); ++i)
        {
            if (statuses[i])
            {
                log("ERORR failed to get snapshot for {}: \n", pair_vec[i], req.get_error_msg(i, statuses[i]));
                continue;
            }
            std::string response {req.get_response(i)};

            Document doc(rapidjson::kObjectType);
            // TODO: check if response string guaranteeded to be null-terminated?
            rapidjson::ParseResult res {doc.ParseInsitu(response.data())};
            if(!res)
            {
                log("ERROR failed to parse snapshot response for {}: {} at offset {:d}", pair_vec[i],
                        rapidjson::GetParseError_En(res.Code()), res.Offset());
                continue;
            }

            if (doc.HasMember("code"))
            {
                log("ERROR snapshot request for {} failed: {}", pair_vec[i], response);
                continue;
            }

            const Value& bids {doc["bids"]};
            const Value& asks {doc["asks"]};
            const Value& last_update_id {doc["lastUpdateId"]};
            if (!last_update_id.IsInt64())
            {
                log("failed to parse lastUpdateId \"{}\"", last_update_id.GetString());
                continue;
            }

            orderbook_t& orderbook = m_orderbooks.at(pair_vec[i]);
            auto& [snapshot_id, _] = m_update_ids[pair_vec[i]];

            snapshot_id = last_update_id.GetInt64();

            orderbook.process_order_snapshot<binance_api>(bids, asks);
        }
    }

    bool message_handler(const Document& payload)
    {
        if (m_get_snapshot)
        {
            process_orderbook_snapsots();
            m_get_snapshot = false;
        }

        if (!payload.HasMember("data"))
        {
            log("unkown message: {}\n", to_string<Document>(payload));;
            return true;
        }

        const Value& json = payload["data"];
        const bool has_type    = json.HasMember("e");

        if (!has_type)
        {
            log("unkown message: {}\n", to_string<Value>(json));
            return true; // ignore and continue listening for more messages
        }

        const auto& type = json["e"];
        if (!std::strncmp("depthUpdate", type.GetString(), type.GetStringLength()))
        {
            process_depth_update(json);
        }
        else if (!std::strncmp("kline", type.GetString(), type.GetStringLength()))
        {
            process_ticker_update(json);
        }
        else
        {
            log("uknown message type: {}\n", to_string<Value>(json));
        }

        return true;
    }

    void notify_event_handlers(feed_event_t::event_type mask, const orderbook_t& book)
    {
        const instrument_pair_t& source_pair{book.pair};
        feed_event_t event {source_pair, mask};
        // notify raw handlers first
        for (auto& [ev, handler_ptr, state] : m_raw_handlers)
        {
            if (!(mask & ev.update_mask) || source_pair != ev.product_pair)
                continue;

            if (!handler_ptr(book, state))
                return;
        }

        // notify callable handlers
        for (auto& [ev, callable] : m_handlers)
        {
            if (!(mask & ev.update_mask) || source_pair != ev.product_pair)
                continue;

            if (!callable(book))
                return;
        }
    }

    void process_ticker_update(const Value& update)
    {
        const auto& symbol = update["s"];
        decltype(m_orderbooks)::iterator it {m_orderbooks.find(std::string{symbol.GetString()})};
        if (it == m_orderbooks.end())
        {
            log("depthUpdate for urecognized symbol {}\n", symbol.GetString());
            return;
        }

        // example kline response:
        //  {
        //  "e": "kline",     // Event type
        //  "E": 123456789,   // Event time
        //  "s": "BNBBTC",    // Symbol
        //  "k": {
        //      "t": 123400000, // Kline start time
        //      "T": 123460000, // Kline close time
        //      "s": "BNBBTC",  // Symbol
        //      "i": "1m",      // Interval
        //      "f": 100,       // First trade ID
        //      "L": 200,       // Last trade ID
        //      "o": "0.0010",  // Open price
        //      "c": "0.0020",  // Close price
        //      "h": "0.0025",  // High price
        //      "l": "0.0015",  // Low price
        //      "v": "1000",    // Base asset volume
        //      "n": 100,       // Number of trades
        //      "x": false,     // Is this kline closed?
        //      "q": "1.0000",  // Quote asset volume
        //      "V": "500",     // Taker buy base asset volume
        //      "Q": "0.500",   // Taker buy quote asset volume
        //      "B": "123456"   // Ignore
        //  }
        //  }
        //

        orderbook_t& orderbook = it->second;
        // orderbook.process_ticker_update<binance_api>(update);
        notify_event_handlers(feed_event_t::TICKER_UPDATED, orderbook);
    }

    void process_depth_update(const Value& update)
    {
        const auto& symbol = update["s"];
        decltype(m_orderbooks)::iterator it {m_orderbooks.find(std::string{symbol.GetString()})};
        if (it == m_orderbooks.end())
        {
            log("depthUpdate for urecognized symbol {}\n", symbol.GetString());
            return;
        }

        orderbook_t& orderbook = it->second;
        const Value& bids = update["b"];
        const Value& asks = update["a"];

        orderbook.process_order_updates<binance_api>(bids, asks);
        notify_event_handlers(feed_event_t::ORDERS_UPDATED, orderbook);
    }
};

#endif
