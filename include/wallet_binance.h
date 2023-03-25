#ifndef _WALLET_BINANCE_H
#define _WALLET_BINANCE_H

#include "exchange_api.h"
#include "requests.h"
#include "json.h"
#include "crypto.h"
#include "wallet.h"

#include <chrono>
#include <ctime>
#include <exception>
#include <optional>
#include <cstring>


template <>
class wallet<binance_api>
{

    struct symbol_info_t
    {
        int quote_precision;
        int base_precision;
        double min_notional;

        double min_price;
        double max_price;
        double step_price;
        double min_qty;
        double max_qty;
        double step_qty;

        Value raw_doc;

        symbol_info_t(int quote_precision, int base_precision, double min_notional,
                      const std::tuple<double, double, double>& price_filter, 
                      const std::tuple<double, double, double>& qty_filter, 
                      Value& raw_doc)
            :// precision
            quote_precision(quote_precision),
            base_precision(base_precision),
            // min volume
            min_notional(min_notional),
            // price filtr
            min_price(std::get<0>(price_filter)),
            max_price(std::get<1>(price_filter)),
            step_price(std::get<2>(price_filter)),
            // qty filter
            min_qty(std::get<0>(qty_filter)),
            max_qty(std::get<1>(qty_filter)),
            step_qty(std::get<2>(qty_filter)),
            // raw json
            raw_doc(std::move(raw_doc))
        { }
    };

private:
    const std::string m_api_key;
    const std::string m_secret_key;
    symbol_info_t m_info;

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


    /**
     * Trys to fetches and parse relevant fields from the exchange info for the pair. Throws exception
     * upon failure.
     *
     * Returns `symbol_info_t` type with information about price and quantity filters (i.e. min/max/step size)
    */
    symbol_info_t load_symbol_info(instrument_pair_t pair)
    {
        const std::string url {std::format("{}{}", binance_api::BASE_API_URL, binance_api::EXCHAGE_INFO_PATH)};

        long time_ms {std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()).time_since_epoch().count()};

        requests_t req;
        req.add_request(url, ReqType::GET)
            .add_header("X-MBX-APIKEY", m_api_key)
            .add_url_param("symbol", instrument_pair::to_binance(pair))
            .add_url_param("timestamp", std::to_string(time_ms));

        CURLcode code = req.fetch_first();
        if (!code)
        {
            throw std::runtime_error(std::format("ERROR exchange info request to {} failed with {}", 
                    exchange_api::to_string(binance_api::exchange_api_id),
                    req.get_error_msg(0, code)));
        }


        std::string response {req.get_response_c_str(0)};

        Document doc;
        rapidjson::ParseResult res {doc.ParseInsitu(response.data())};

        if (!res)
        {
            throw std::runtime_error(std::format("ERROR get order response \"{}\", parse error at offet {}: to parse: {}", response,
                    res.Offset(), rapidjson::GetParseError_En(res.Code())));
        }

        if (!doc.HasMember("symbols") && !doc["symbols"].IsArray() && doc["symbols"].Size() != 1)
        {
            throw std::runtime_error(std::format("ERROR get order unkown response: {}", to_string<Document>(doc)));
        }

        Value& symbol = doc["symbols"][0];

        int quote_precision = symbol["quotePrecision"].GetInt();
        int base_precision = symbol["baseAssetPrecision"].GetInt();
        double min_notional = -1;

        bool lot_size_filter_found = false;
        bool price_filter_found    = false;
        std::tuple<double, double, double> lot_size_filter;
        std::tuple<double, double, double> price_filter;

        Value& filters = doc["filters"];
        for (size_t i = 0; i < filters.Size(); ++i)
        {
            const Value& filter = filters[i];
            const char* ftype = filter["filterType"].GetString();
            const size_t flen = filter["filterType"].GetStringLength();

            if (min_notional < 0 && !std::strncmp("MIN_NOTIONAL", ftype, flen))
            {
                min_notional = std::stod(filter["minNotional"].GetString());
            }
            else if (!lot_size_filter_found && !std::strncmp("LOT_SIZE", ftype, flen))
            {
                lot_size_filter = { get_member_from_str<double>(filter, "minQty"),
                                    get_member_from_str<double>(filter, "maxQty"),
                                    get_member_from_str<double>(filter, "stepSize") };
                lot_size_filter_found = true;
            }
            else if (!price_filter_found && !std::strncmp("PRICE_FILTER", ftype, flen))
            {
                price_filter = { get_member_from_str<double>(filter, "minPrice"),
                                    get_member_from_str<double>(filter, "maxPrice"),
                                    get_member_from_str<double>(filter, "tickSize") };
                price_filter_found = true;
            }
        }

        if (min_notional < 0 || !lot_size_filter_found || !price_filter_found)
            throw std::runtime_error(std::format("ERORR unable to all filter values: {}", to_string<Value>(filters)));


        return symbol_info_t(quote_precision, base_precision, min_notional, price_filter, lot_size_filter, symbol);
    }


public:
    const instrument_pair_t pair;

    wallet<>(instrument_pair_t pair, const std::string& api_key, const std::string& secret_key)
        :  m_api_key {api_key}, m_secret_key{secret_key}, m_info(load_symbol_info(pair)), pair{pair}
    { }



    void create_limit_order_request(requests_t& req, SIDE side, double limit_price, double quantity)
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

        Document doc;
        std::string response {req.get_response_c_str(index)};
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
        create_limit_order_request(req, SIDE::SELL, limit_price, quantity);
        req.fetch_all(codes);
        return parse_create_limit_order_request(req, codes, 0);
    }

    std::optional<order_status> create_limit_buy_order(instrument_pair_t pair, double limit_price, double quantity)
    {
        requests_t req;
        requests_t::statuses_t codes;
        create_limit_order_request(req, SIDE::BUY, limit_price, quantity);
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
