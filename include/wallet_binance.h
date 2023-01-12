#ifndef _WALLET_BINANCE_H
#define _WALLET_BINANCE_H

#include "exchange_api.h"
#include "requests.h"
#include "json.h"
#include "crypto.h"
#include "wallet.h"

#include <chrono>
#include <ctime>
#include <optional>
#include <cstring>


template <>
class wallet<binance_api>
{
private:
    const std::string m_api_key;
    const std::string m_secret_key;

    std::string sign_payload(const request_args_t& args, const std::string& payload)
    {
        std::stringstream sig_plain;
        sig_plain << args.url_params_to_string() << payload;

        std::string digest;
        hmac(sig_plain.str(), m_secret_key, digest);
        log("signature_plain: {}, digest: {}", sig_plain.str(), digest);

        return digest;
    }

    bool try_cancel_limit_order(const std::string& order_id)
    {
        const std::string url {std::format("{}{}", binance_api::BASE_API_URL, binance_api::CANCEL_ORDER_PATH)};

        long time_ms {std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()).time_since_epoch().count()};

        requests_t req;
        request_args_t& rargs = req.add_request(url, ReqType::POST)
            .add_header("X-MBX-APIKEY", m_api_key)
            .add_header("Connection", "close")
            .add_url_param("orderId", order_id)
            .add_url_param("recvWindow", std::to_string(5000))
            .add_url_param("timestamp", std::to_string(time_ms));

        std::string signature {sign_payload(rargs, "")};
        rargs.add_url_param("signature", signature);


        requests_t::statuses_t codes;
        size_t failed = req.fetch_all(codes);

        if (failed > 0 || codes.at(0))
        {
            log("ERROR cancel order request to {} failed with {}", 
                    exchange_api::to_string(binance_api::exchange_api_id),
                    req.get_error_msg(0, codes[0]));

            return false;
        }

        std::string response {req.get_response(0)};
        if (response.back() != '\0')
            response.push_back('\0');

        Document doc;
        rapidjson::ParseResult res {doc.ParseInsitu(response.data())};

        if (!res)
        {
            log("ERROR cancel order response \"{}\", parse error at offet {}: to parse: {}", response,
                    res.Offset(), rapidjson::GetParseError_En(res.Code()));
            return false;
        }

        if (!doc.HasMember("status"))
        {
            log("ERROR unkown response: {}", to_string<Document>(doc));
            return false;
        }


        const Value& status = doc["status"];
        if (std::strncmp("CANCELLED", status.GetString(), status.GetStringLength()))
        {
            log("ERROR failed to canceled order: {}", to_string<Document>(doc));
            return false;
        }

        return true;
    }


public:
    wallet<>(const std::string& api_key, const std::string& secret_key)
        : m_api_key {api_key}, m_secret_key{secret_key}
    { }



    void create_limit_order_request(requests_t& req, SIDE side, instrument_pair_t pair, double limit_price, double quantity)
    {
        const std::string url {std::format("{}{}", binance_api::BASE_API_URL, binance_api::CREATE_ORDER_PATH)};

        long time_ms {std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()).time_since_epoch().count()};

        std::string limit_price_str { side == SIDE::BUY ? round_bid_price_to_precision(limit_price, 4) : round_ask_price_to_precision(limit_price, 4)};
        request_args_t& rargs = req.add_request(url, ReqType::POST)
            .add_header("X-MBX-APIKEY", m_api_key)
            .add_header("Connection", "close")
            .add_url_param("symbol", instrument_pair::to_binance(pair))
            .add_url_param("side", order_status::side_to_string(side))
            .add_url_param("type", "LIMIT")
            .add_url_param("quantity", round_quantity_to_precision(quantity, 4))
            .add_url_param("timeInForce", "IOC")
            //.add_url_param("stopLimitTimeInForce", std::to_string(5000))
            .add_url_param("price", limit_price_str)
            .add_url_param("recvWindow", std::to_string(5000))
            .add_url_param("timestamp", std::to_string(time_ms));

        std::string signature {sign_payload(rargs, "")};
        rargs.add_url_param("signature", signature);
    }


    std::optional<order_status> parse_create_limit_order_request(requests_t& req, requests_t::statuses_t& statues, size_t index)
    {
        if (statues.at(index) > 0)
        {
            log("ERROR create limit order request to {} failed with {}", 
                    exchange_api::to_string(binance_api::exchange_api_id),
                    req.get_error_msg(index, statues[0]));

            return std::nullopt;
        }


        std::string response {req.get_response(index)};
        if (response.back() != '\0')
            response.push_back('\0');


        Document doc;
        rapidjson::ParseResult res {doc.ParseInsitu(response.data())};

        if (!res)
        {
            log("ERROR create limit order response \"{}\", parse error at offet {}: to parse: {}", response,
                    res.Offset(), rapidjson::GetParseError_En(res.Code()));
            return std::nullopt;
        }

        if (!doc.HasMember("symbol") && !doc.HasMember("orderId"))
        {
            log("ERROR unkown response: {}", to_string<Document>(doc));
            return std::nullopt;
        }

        std::string order_id {doc["orderId"].GetString()};
        STATUS status = order_status::status_from_string(get_json_string(doc, "status").value_or(""));
        SIDE side = order_status::side_from_string(get_json_string(doc, "side").value_or(""));

        log("SUCCESS created order: {}", to_string<Document>(doc));

        return std::optional<order_status>(std::in_place, order_id, side, status);
    }


    std::optional<order_status> create_limit_sell_order(instrument_pair_t pair, double limit_price, double quantity)
    {
        requests_t req;
        requests_t::statuses_t codes;
        create_limit_order_request(req, SIDE::SELL, pair, limit_price, quantity);
        req.fetch_all(codes);
        return parse_create_limit_order_request(req, codes, 0);
    }

    std::optional<order_status> create_limit_buy_order(instrument_pair_t pair, double limit_price, double quantity)
    {
        requests_t req;
        requests_t::statuses_t codes;
        create_limit_order_request(req, SIDE::BUY, pair, limit_price, quantity);
        req.fetch_all(codes);

        return parse_create_limit_order_request(req, codes, 0);
    }


    std::optional<order_status> get_order(const instrument_pair_t& pair, const std::string& order_id)
    {
        const std::string url {std::format("{}{}", binance_api::BASE_API_URL, binance_api::GET_ORDER_PATH)};

        long time_ms {std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()).time_since_epoch().count()};

        requests_t req;
        request_args_t& rargs = req.add_request(url, ReqType::GET)
            .add_header("X-MBX-APIKEY", m_api_key)
            .add_header("Connection", "close")
            .add_url_param("symbol", instrument_pair::to_binance(pair))
            .add_url_param("orderId", order_id)
            .add_url_param("recvWindow", std::to_string(6000))
            .add_url_param("timestamp", std::to_string(time_ms));

        std::string signature {sign_payload(rargs, "")};
        rargs.add_url_param("signature", signature);

        requests_t::statuses_t codes;
        size_t failed = req.fetch_all(codes);
        if (failed > 0 || codes.at(0) > 0)
        {
            log("ERROR get order request to {} failed with {}", 
                    exchange_api::to_string(binance_api::exchange_api_id),
                    req.get_error_msg(0, codes[0]));

            return std::nullopt;
        }


        std::string response {req.get_response(0)};
        if (response.back() != '\0')
            response.push_back('\0');


        Document doc;
        rapidjson::ParseResult res {doc.ParseInsitu(response.data())};

        if (!res)
        {
            log("ERROR get order response \"{}\", parse error at offet {}: to parse: {}", response,
                    res.Offset(), rapidjson::GetParseError_En(res.Code()));
            return std::nullopt;
        }

        if (!doc.HasMember("orderId") && !doc.HasMember("symbol") && !doc.HasMember("status"))
        {
            log("ERROR get order unkown response: {}", to_string<Document>(doc));
            return std::nullopt;
        }

        STATUS status = order_status::status_from_string(get_json_string(doc, "status").value_or(""));
        SIDE side = order_status::side_from_string(get_json_string(doc, "side").value_or(""));

        log("SUCCESS get order {}: {}", order_id, to_string<Document>(doc));

        return std::optional<order_status>(std::in_place, order_id, side, status);
    }


    std::optional<double> get_asset_account_balance(std::string currency = "USD")
    {
        const std::string url {std::format("{}{}", binance_api::BASE_API_URL, binance_api::GET_ACCOUNT_PATH)};

        long time_ms {std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()).time_since_epoch().count()};

        requests_t req;
        request_args_t& rargs = req.add_request(url, ReqType::GET)
            .add_header("X-MBX-APIKEY", m_api_key)
            .add_header("Connection", "close")
            //.add_url_param("recvWindow", std::to_string(6000))
            .add_url_param("timestamp", std::to_string(time_ms));

        std::string signature {sign_payload(rargs, "")};
        rargs.add_url_param("signature", signature);

        requests_t::statuses_t codes;
        size_t failed = req.fetch_all(codes);
        if (failed > 0 || codes.at(0) > 0)
        {
            log("ERROR get account request to {} failed with {}", 
                    exchange_api::to_string(binance_api::exchange_api_id),
                    req.get_error_msg(0, codes[0]));

            return std::nullopt;
        }


        std::string response {req.get_response(0)};
        if (response.back() != '\0')
            response.push_back('\0');


        Document doc;
        rapidjson::ParseResult res {doc.ParseInsitu(response.data())};

        if (!res)
        {
            log("ERROR get order response \"{}\", parse error at offet {}: to parse: {}", response,
                    res.Offset(), rapidjson::GetParseError_En(res.Code()));
            return std::nullopt;
        }

        if (!doc.HasMember("balances") || !doc["balances"].IsArray())
        {
            log("ERROR get account unkown response: {}", to_string<Document>(doc));
            return std::nullopt;
        }

        const Value& balances = doc["balances"];
        for (size_t i = 0; i < balances.Size(); ++i)
        {
            const Value& balance = balances[i];
            if (!balance.HasMember("asset") || !balance.HasMember("free") || !balance.HasMember("locked"))
            {
                log("ERROR unexpected balance {}", to_string<Value>(balance));
                continue;
            }

            std::string asset {balance["asset"].GetString()};
            double free {std::stod(balance["free"].GetString())};
            double locked {std::stod(balance["locked"].GetString())};
            log("{} balance: free {:f}, locked {:f}", asset, free, locked);

            if (asset == currency)
                return std::optional<double>(free);
        }

        return std::nullopt;
    }

    bool cancel_limit_order(const std::string& order_id, size_t attempts = 1)
    {
        while (attempts > 0)
        {
            if (try_cancel_limit_order(order_id))
                return true;
            attempts -= 1;
        }

        return false;
    }


};

#endif
