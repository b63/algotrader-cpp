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
          m_handlers{},
          m_raw_handlers{}
    {
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
        std::string json_str {to_string<Document>(json)};
        log("received message: {}\n", json_str.substr(0, MIN(100, json_str.size())));
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
