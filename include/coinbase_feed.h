#ifndef _COINBASE_FEED_H
#define _COINBASE_FEED_H

#include "exchange_api.h"
#include "market_socket.h"
#include "json.h"
#include "logger.h"
#include "crypto.h"

#include <string>
#include <thread>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <functional>
#include <algorithm>
#include <utility>


template <>
class market_feed<coinbase_api>
{
public:
    market_feed<coinbase_api>(const std::vector<instrument_pair_t>& pairs,
            const std::string& api_key, const std::string& secret_key)
        : m_pairs {pairs}, m_channels {"level2", "ticker"},
          m_secret_key {secret_key},
          m_api_key {api_key},
          m_socket{nullptr},
          m_thread {nullptr},
          m_stop_source {},
          m_orderbooks{},
          m_handlers{},
          m_raw_handlers{}
    {
        for (auto& pair : m_pairs)
        {
            m_orderbooks.emplace(std::piecewise_construct,
                    std::forward_as_tuple(instrument_pair::to_coinbase(pair)),
                    std::forward_as_tuple(pair, coinbase_api::exchange_api_id));
        }
    }

    void start_feed()
    {
        if (m_thread && m_thread->joinable())
            throw std::runtime_error("start_feed called when another is already running in another thread");

        const std::string uri {""};
        m_socket = std::make_unique<market_feed_socket>(coinbase_api::SOCKET_URI, 
                std::bind(&market_feed<coinbase_api>::message_handler, this, std::placeholders::_1));

        add_subscribe_messages();

        m_thread = std::make_unique<std::jthread>(&market_feed<coinbase_api>::_start_feed, this, m_stop_source.get_token());
    }

    void join()
    {
        if (m_thread)
            m_thread->join();
    }

    void close()
    {
        if (!m_socket) return;
        // TODO: send unsubscribe message before closing?

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
    std::array<std::string,2>            m_channels;
    std::string                          m_secret_key;
    std::string                          m_api_key;
    std::unique_ptr<market_feed_socket>  m_socket;
    std::unique_ptr<std::jthread>        m_thread;
    std::stop_source                     m_stop_source;
    std::unordered_map<std::string, orderbook_t> m_orderbooks;

    std::vector<std::tuple<feed_event_t, feed_event_handler_t>>               m_handlers;
    std::vector<std::tuple<feed_event_t, feed_event_handler_ptr, std::any>> m_raw_handlers;

    void _start_feed(const std::stop_token &stop_token)
    {
        if (!m_socket)
            throw std::runtime_error("_start_feed called in invalid state");
        std::error_code ec;
        m_socket->connect(ec);
    }

    bool message_handler(const Document& json)
    {
        const bool has_channel = json.HasMember("channel");
        const bool has_type    = json.HasMember("type");
        if (!has_type && !has_channel)
        {
            log("unkown message: {}\n", to_string<Document>(json));
            return true; // ignore and continue listening for more messages
        }

        if (has_type)
        {
            const auto& type = json["type"];
            if (!std::strncmp(type.GetString(), "error", type.GetStringLength()))
            {
                // received error message
                if (!json.HasMember("message"))
                    log("error response: {}", to_string<Document>(json));
                else
                    log("error response: {}", std::string(json["message"].GetString()));
                // close down the websocket
                return false;
            }
            log("uknown message type: {}\n", to_string<Document>(json));
            return true;
        }

        const auto& channel = json["channel"];
        // int sequence_num = json["sequence_num"].GetInt();
        // #TODO: ensure that sequence_num increments by one every message
        if (!std::strncmp("l2_data", channel.GetString(), channel.GetStringLength()))
        {
            // maket feed data
            process_l2_data_events(json["events"]);
        }
        else if (!std::strncmp("ticker", channel.GetString(), channel.GetStringLength()))
        {
            // ticker data
            process_tickers_data_events(json["events"]);
        }
        else if (!std::strncmp("subscriptions", channel.GetString(), channel.GetStringLength()))
        {
            log("received subscription response: {}", to_string<Document>(json));
        }
        else
        {
            log("unkown channel: {}", to_string<Document>(json));
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
            if (!(mask & ev.update_mask) || source_pair != ev.product_pair))
                continue;

            if (!handler_ptr(book, state))
                return;
        }

        // notify callable handlers
        for (auto& [ev, callable] : m_handlers)
        {
            if (!(mask & ev.update_mask) || source_pair != ev.product_pair))
                continue;

            if (!callable(book))
                return;
        }
    }

    void process_tickers_data_events(const Value& events)
    {
        // assert(events.IsArray())
        for (size_t i = 0; i < events.Size(); ++i)
        {
            const Value& event      = events[i];
            const Value& type       = event["type"];
            const bool is_update   = std::strncmp("update", type.GetString(), type.GetStringLength());
            const bool is_snapshot = !is_update && std::strncmp("snapshot", type.GetString(), type.GetStringLength());
            if (!is_update && !is_snapshot)
            {
                log("unkown event type: {}\n", type.GetString());
                continue;
            }

            const Value& tickers = event["tickers"];

            for(size_t j = 0; j < tickers.Size(); ++j)
            {
                const Value& ticker = tickers[j];
                // assert(!strncmp("ticker", ticker["type"].GetString(), ticker["type"].GetStringLength())
                const Value& product_id = ticker["product_id"];

                decltype(m_orderbooks)::iterator key_val = m_orderbooks.find(std::string{product_id.GetString()});
                if (key_val == m_orderbooks.end())
                    continue;

                /**
                 * example ticker json object:
                 * {
                 *     "type": "ticker",
                 *     "product_id": "ETH-USD",
                 *     "price": "1223.3",
                 *     "volume_24_h": "200037.55097283",
                 *     "low_24_h": "1204.04",
                 *     "high_24_h": "1228.61",
                 *     "low_52_w": "879.8",
                 *     "high_52_w": "3894.12",
                 *     "price_percent_chg_24_h": "0.87241902500165"
                 *  }
                 */
                orderbook_t& orderbook = key_val->second;
                orderbook.process_ticker_update<coinbase_api>(ticker);
                notify_event_handlers(feed_event_t::TICKER_UPDATED, orderbook);
            }

        }
    }

    void process_l2_data_events(const Value& events)
    {
        // assert(events.IsArray())
        for (size_t i = 0; i < events.Size(); ++i)
        {
            // TODO: either check for existence of memebers before using [] on Value types
            //       or have a enclosing try-catch somewhere
            const Value& event      = events[i];
            const Value& type       = event["type"];
            const Value& product_id = event["product_id"];

            decltype(m_orderbooks)::iterator key_val = m_orderbooks.find(std::string{product_id.GetString()});
            if (key_val == m_orderbooks.end())
                continue;
            orderbook_t& orderbook = key_val->second;

            if (!std::strncmp("update", type.GetString(), type.GetStringLength()))
            {
                orderbook.process_order_updates<coinbase_api>(event["updates"]);
                notify_event_handlers(feed_event_t::ORDERS_UPDATED, orderbook);
            }
            else if (!std::strncmp("snapshot", type.GetString(), type.GetStringLength()))
            {
                orderbook.process_order_snapshot<coinbase_api>(event["updates"]);
                notify_event_handlers(feed_event_t::ORDERS_UPDATED, orderbook);
            }
            else
            {
                log("unkown event type: {}\n", type.GetString());
            }
        }
    }

    void add_subscribe_messages()
    {
        using namespace rapidjson;
        Document doc(Type::kObjectType);
        auto& alloc = doc.GetAllocator();

        Value pairs(kArrayType);
        for (const auto& pair : m_pairs)
        {
            std::string pair_str {instrument_pair::to_coinbase(pair)};
            pairs.PushBack(Value().SetString(pair_str.c_str(), pair_str.length(), alloc), alloc);
        }

        doc.AddMember("type", Value().SetString("subscribe"), alloc)
           .AddMember("product_ids", pairs, alloc)
           .AddMember("user_id", Value().SetString(""), alloc)
           .AddMember("api_key", Value().SetString(m_api_key.c_str(), alloc), alloc);

        for (const auto& channel : m_channels)
        {
            add_or_overwrite_member(doc, "channel", Value().SetString(channel.c_str(), channel.length(), alloc), alloc);
            time_stamp_and_sign(doc, channel);

            m_socket->add_opening_message_json(doc);
        }
    }


    void time_stamp_and_sign(Document& msg, const std::string& channel)
    {
        using namespace std::chrono;
        std::stringstream sig_plain;

        auto tp {time_point_cast<seconds>(system_clock::now())};
        std::string ts {std::to_string(tp.time_since_epoch().count())};
        sig_plain << ts;


        sig_plain << channel;

        for (int i = 0; i < m_pairs.size(); ++i)
        {
            sig_plain << instrument_pair::to_coinbase(m_pairs[i]);
            if (i+1 != m_pairs.size()) 
                sig_plain << ",";
        }


        std::string digest;
        hmac(sig_plain.str(), m_secret_key, digest);
        log("plain: {}, digest: {}", sig_plain.str(), digest);

        add_or_overwrite_member(msg, "signature", Value().SetString(digest.c_str(), digest.length(), msg.GetAllocator()), msg.GetAllocator());
        add_or_overwrite_member(msg, "timestamp", Value().SetString(ts.c_str(), ts.length(), msg.GetAllocator()), msg.GetAllocator());
    }
};

#endif
