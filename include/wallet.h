#ifndef _WALLET_H
#define _WALLET_H

#include "exchange_api.h"
#include "requests.h"
#include "json.h"
#include "crypto.h"

#include <chrono>
#include <optional>

template <typename ExchangeAPI>
    requires is_exchange_api<ExchangeAPI>
class wallet {};

template <typename T>
concept is_wallet = is_any<T, wallet<coinbase_api>, wallet<binance_api>>;


template <>
class wallet<coinbase_api>
{
private:
    const std::string m_api_key;
    const std::string m_secret_key;

public:
    wallet<>(const std::string& api_key, const std::string& secret_key)
        : m_api_key {api_key}, m_secret_key{secret_key}
    { }

    std::string generate_order_uuid()
    {
        std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds> now {std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now())};

        return std::to_string(now.time_since_epoch().count());
    }


    std::string sign_payload(const std::string& request_path, const std::string& payload, long time_seconds)
    {
        std::stringstream sig_plain;
        sig_plain << std::to_string(time_seconds) << "POST" << request_path << payload;

        std::string digest;
        hmac(sig_plain.str(), m_secret_key, digest);

        // TODO: check for copy-elision
        return digest;
    }

    bool create_immediate_sell_order(instrument_pair_t pair, double price, std::string& order_id_ref)
    {
        const std::string url {std::format("{}{}", coinbase_api::BASE_API_URL, coinbase_api::CREATE_ORDER_PATH)};

        // see API reference: https://docs.cloud.coinbase.com/advanced-trade-api/reference/retailbrokerageapi_postorder
        DocumentCreator dc;
        auto& alloc = dc.doc.GetAllocator();

        Value object_configuration (rapidjson::kObjectType);
        Value market_maket_ioc (rapidjson::kObjectType);
        market_maket_ioc.AddMember("base_size", Value().SetString(std::to_string(price).c_str(), alloc), alloc);

        // immediate-or-cancel sell-order
        object_configuration.AddMember("market_market_ioc", market_maket_ioc, alloc);
        dc.AddString("client_order_id", generate_order_uuid()); // TODO: make use of uuid?
        dc.AddString("product_id", instrument_pair::to_coinbase(pair));
        dc.AddString("side", "SELL");

        std::string payload;
        dc.write_string(payload);

        long time_seconds {std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch().count()};
        std::string signature {sign_payload(coinbase_api::CREATE_ORDER_PATH, payload, time_seconds)};

        requests_t req;
        req.add_request(url, ReqType::POST)
            .add_header("accept", "application/json")
            .add_header("cb-access-key", m_api_key)
            .add_header("cb-access-timestamp", std::to_string(time_seconds))
            .add_header("cb-access-sign", signature)
            .set_data(dc.as_string());

        std::vector<CURLcode> statues;
        size_t failed = req.fetch_all(statues);

        if (failed > 0)
        {
            log("ERROR in create_immediate_sell_order: reques to {} failed with {}", 
                    exchange_api::to_string(coinbase_api::exchange_api_id),
                    req.get_error_msg(0, statues[0]));

            return false;
        }

        std::string response {req.get_response(0)};
        if (response.back() != '\0')
            response.push_back('\0');

        Document doc;
        doc.ParseInsitu(response.data());

        const bool success = doc["string"].GetBool();
        if (!success)
        {
            const Value& reason = doc["failure_reason"];
            log("ERROR in create_immediate_sell_order: request to {} received failed response; failure_reason: {}", 
                    exchange_api::to_string(coinbase_api::exchange_api_id),
                    reason.GetString());
            return false;
        }

        const Value& order_id = doc["order_id"];
        log("SUCCESS create_immediate_sell_order to {} succeeded, order_id: {}", 
                exchange_api::to_string(coinbase_api::exchange_api_id),
                order_id.GetString());

        order_id_ref = std::string{order_id.GetString()};

        return true;
    }


    bool create_immediate_buy_order(instrument_pair_t pair, double price, std::string& order_id_ref)
    {
        const std::string url {std::format("{}{}", coinbase_api::BASE_API_URL, coinbase_api::CREATE_ORDER_PATH)};

        // see API reference: https://docs.cloud.coinbase.com/advanced-trade-api/reference/retailbrokerageapi_postorder
        DocumentCreator dc;
        auto& alloc = dc.doc.GetAllocator();

        Value object_configuration (rapidjson::kObjectType);
        Value market_maket_ioc (rapidjson::kObjectType);
        market_maket_ioc.AddMember("quote_size", Value().SetString(std::to_string(price).c_str(), alloc), alloc);

        // immediate-or-cancel sell-order
        object_configuration.AddMember("market_market_ioc", market_maket_ioc, alloc);
        dc.AddString("client_order_id", generate_order_uuid()); // TODO: make use of uuid?
        dc.AddString("product_id", instrument_pair::to_coinbase(pair));
        dc.AddString("side", "BUY");

        std::string payload;
        dc.write_string(payload);

        long time_seconds {std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch().count()};
        std::string signature {sign_payload(coinbase_api::CREATE_ORDER_PATH, payload, time_seconds)};

        requests_t req;
        req.add_request(url, ReqType::POST)
            .add_header("accept", "application/json")
            .add_header("cb-access-key", m_api_key)
            .add_header("cb-access-timestamp", std::to_string(time_seconds))
            .add_header("cb-access-sign", signature)
            .set_data(dc.as_string());

        std::vector<CURLcode> statues;
        size_t failed = req.fetch_all(statues);

        if (failed > 0)
        {
            log("ERROR in create_immediate_by_order: request to {} failed with {}", 
                    exchange_api::to_string(coinbase_api::exchange_api_id),
                    req.get_error_msg(0, statues[0]));

            return false;
        }

        std::string response {req.get_response(0)};
        if (response.back() != '\0')
            response.push_back('\0');

        Document doc;
        doc.ParseInsitu(response.data());

        const bool success = doc["string"].GetBool();
        if (!success)
        {
            const Value& reason = doc["failure_reason"];
            log("ERROR: create_immediate_buy_order to {} failed; failure_reason: {}", 
                    exchange_api::to_string(coinbase_api::exchange_api_id),
                    reason.GetString());
            return false;
        }

        const Value& order_id = doc["order_id"];
        log("SUCCESS: create_immediate_buy_order to {} succeeded, order_id: {}", 
                exchange_api::to_string(coinbase_api::exchange_api_id),
                order_id.GetString());

        order_id_ref = std::string{order_id.GetString()};

        return true;
    }


    bool get_order(const std::string& order_id, Document& doc)
    {
        const std::string request_path{std::format("{}/{}/", coinbase_api::GET_ORDER_PATH, order_id)};

        long time_seconds {std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch().count()};
        std::string signature {sign_payload(request_path, "", time_seconds)};

        requests_t req;
        req.add_request(coinbase_api::BASE_API_URL + request_path, ReqType::GET)
            .add_header("accept", "application/json")
            .add_header("cb-access-key", m_api_key)
            .add_header("cb-access-timestamp", std::to_string(time_seconds))
            .add_header("cb-access-sign", signature);

        std::vector<CURLcode> statues;
        size_t failed = req.fetch_all(statues);
        if (failed > 0)
        {
            log("ERROR: create_immediate_by_order to {} failed with {}", 
                    exchange_api::to_string(coinbase_api::exchange_api_id),
                    req.get_error_msg(0, statues[0]));

            return false;
        }

        doc.Parse(req.get_response(0).c_str());

        return true;
    }
};

#endif
