#ifndef _WALLET_COINBASE_H
#define _WALLET_COINBASE_H

#include "exchange_api.h"
#include "crypt.h"
#include "json.h"
#include "wallet.h"

template <>
class wallet<coinbase_api>
{
private:
    const std::string m_api_key;
    const std::string m_secret_key;

    std::string generate_order_uuid()
    {
        std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds> now {std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now())};

        return std::to_string(now.time_since_epoch().count());
    }


    std::string time_string(long delta)
    {
        std::time_t time {std::time(nullptr)+delta};
        char tstring[std::size("yyyy-mm-ddThh:mm:ss+00:00")];
        std::strftime(std::data(tstring), std::size(tstring), "%FT%H:%M:%S+00:00", std::gmtime(&time));

        return std::string{&tstring[0]};
    }


    std::string sign_payload(const std::string& request_path, const std::string& payload, long time_seconds, const std::string method = "POST")
    {
        std::stringstream sig_plain;
        sig_plain << std::to_string(time_seconds) << method << request_path << payload;

        std::string digest;
        hmac(sig_plain.str(), m_secret_key, digest);
        //log("signature_plain: {}, digest: {}", sig_plain.str(), digest);

        // TODO: check for copy-elision
        return digest;
    }

    enum class cancel_order_code : int {
        OK = 0,
        FAILED = 1,
        UNKNOWN_ORDER = 2
    };

    cancel_order_code try_cancel_limit_order(const std::string& order_id)
    {
        const std::string url {std::format("{}{}", coinbase_api::BASE_API_URL, coinbase_api::CANCEL_ORDER_PATH)};

        // see API reference: https://docs.cloud.coinbase.com/advanced-trade-api/reference/retailbrokerageapi_postorder
        DocumentCreator<Document> dc;
        auto& alloc = dc.doc.GetAllocator();

        dc.doc.AddMember("order_ids", 
            Value(rapidjson::kArrayType)
                .PushBack(Value().SetString(order_id.c_str(), alloc), alloc), alloc);

        std::string payload;
        dc.write_string(payload);

        long time_seconds {(std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now())).time_since_epoch().count()};
        std::string signature {sign_payload(coinbase_api::CANCEL_ORDER_PATH, payload, time_seconds)};

        requests_t req;
        req.add_request(url, ReqType::POST)
            .add_header("accept", "application/json")
            .add_header("CB-ACCESS-KEY", m_api_key)
            .add_header("CB-ACCESS-SIGN", signature)
            .add_header("CB-ACCESS-TIMESTAMP", std::to_string(time_seconds))
            .set_data(payload);

        std::vector<CURLcode> statues;
        size_t failed = req.fetch_all(statues);

        if (failed > 0)
        {
            log("ERROR in cancel_limit_order: request to {} failed with {}", 
                    exchange_api::to_string(coinbase_api::exchange_api_id),
                    req.get_error_msg(0, statues[0]));

            return cancel_order_code::FAILED;
        }

        std::string response {req.get_response(0)};
        if (response.back() != '\0')
            response.push_back('\0');

        Document doc;
        rapidjson::ParseResult res (doc.ParseInsitu(response.data()));
        if (!res)
        {
            log("ERROR cancel_limit_order response \"{}\", parse error at offet {}: to parse: {}", response,
                    res.Offset(), rapidjson::GetParseError_En(res.Code()));
            return cancel_order_code::FAILED;
        }

        if (!doc.HasMember("results"))
        {
            log("ERROR unkown response (expected 'results' member): {}", to_string<Document>(doc));
            return cancel_order_code::FAILED;
        }

        bool canceled = false;
        const Value& results = doc["results"];
        for (size_t i = 0; i < results.Size(); ++i)
        {
            const Value& result = results[i];
            if (!result.HasMember("order_id"))
                continue;
            const Value& res_order_id = result["order_id"];
            if (std::strncmp(order_id.c_str(), res_order_id.GetString(), res_order_id.GetStringLength()))
                continue;

            if (result.HasMember("success") && result["success"].GetBool())
            {
                canceled = true;
                break;
            }

            if (!result.HasMember("failure_reason"))
                continue;
            const Value& reason = result["failure_reason"];
            if (!std::strncmp("UNKNOWN_CANCEL_ORDER", reason.GetString(), reason.GetStringLength()))
            {
                return cancel_order_code::UNKNOWN_ORDER;
            }
        }

        if (canceled)
            return cancel_order_code::OK;

        log("failed to cancel order_id {}: {}", order_id, to_string<Document>(doc));
        return cancel_order_code::FAILED;
    }

public:
    wallet<>(const std::string& api_key, const std::string& secret_key)
        : m_api_key {api_key}, m_secret_key{secret_key}
    { }

    void create_limit_order_request(requests_t& req, const std::string& side, instrument_pair_t pair, double limit_price, double quantity)
    {
        const std::string url {std::format("{}{}", coinbase_api::BASE_API_URL, coinbase_api::CREATE_ORDER_PATH)};

        // see API reference: https://docs.cloud.coinbase.com/advanced-trade-api/reference/retailbrokerageapi_postorder
        DocumentCreator<Document> dc;
        auto& alloc = dc.doc.GetAllocator();

        dc.doc.AddMember("client_order_id", Value().SetString(generate_order_uuid().c_str(), alloc), alloc); // TODO: make use of uuid?
        dc.doc.AddMember("product_id", Value().SetString(instrument_pair::to_coinbase(pair).c_str(), alloc), alloc);
        dc.doc.AddMember("side", Value().SetString(side.c_str(), alloc), alloc);

        Value order_config (rapidjson::kObjectType);
        order_config.AddMember("limit_limit_gtd",
                Value(rapidjson::kObjectType)
                    .AddMember("base_size",   Value().SetString(std::to_string(quantity).c_str(), alloc),    alloc)
                    .AddMember("limit_price", Value().SetString(std::to_string(limit_price).c_str(), alloc), alloc)
                    .AddMember("end_time",    Value().SetString(time_string(10).c_str(), alloc),              alloc)
                    .AddMember("post_only",   Value().SetBool(false), alloc),
                alloc);
        // NOTE: using end_time < now + 4sec fails with "order_config argument is invalid"
        //       instead just send cancel request immedieatly afterto get IOC-like behavior
        dc.doc.AddMember("order_configuration", order_config, alloc);

        std::string payload;
        dc.write_string(payload);

        long time_seconds {std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch().count()};
        std::string signature {sign_payload(coinbase_api::CREATE_ORDER_PATH, payload, time_seconds)};

        req.add_request(url, ReqType::POST)
            .add_header("accept", "application/json")
            .add_header("CB-ACCESS-KEY", m_api_key)
            .add_header("CB-ACCESS-SIGN", signature)
            .add_header("CB-ACCESS-TIMESTAMP", std::to_string(time_seconds))
            .set_data(payload);

    }

    bool parse_create_limit_order_request(requests_t& req, requests_t::statuses_t& statues, size_t index, std::string& order_id_ref)
    {
        // log("create_immediate_{}_order: sending request {}", side, payload);
        if (statues.at(index) > 0)
        {
            log("ERROR create limit order request to {} failed with {}", 
                    exchange_api::to_string(coinbase_api::exchange_api_id),
                    req.get_error_msg(index, statues[0]));

            return false;
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
            return false;
        }

        if (!doc.HasMember("success"))
        {
            log("ERROR unkown response (expected 'success' member): {}", to_string<Document>(doc));
            return false;
        }

        const bool success = doc["success"].GetBool();
        if (!success)
        {
            const Value& reason = doc["failure_reason"];
            log("ERROR in create limit order: request to {} received failed response; failure_reason: {}", 
                    exchange_api::to_string(coinbase_api::exchange_api_id),
                    reason.GetString());
            return false;
        }

        const Value& order_id = doc["order_id"];
        log("SUCCESS create limit order to {} succeeded, order_id: {}", 
                exchange_api::to_string(coinbase_api::exchange_api_id),
                order_id.GetString());

        order_id_ref = std::string{order_id.GetString()};

        return true;
    }

    std::optional<Document> get_account(const std::string& uuid)
    {
        const std::string request_path{std::format("{}/{}", coinbase_api::GET_ACCOUNT_PATH, uuid)};

        long time_seconds {std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch().count()};
        std::string signature {sign_payload(request_path, "", time_seconds, "GET")};

        requests_t req;
        req.add_request(std::format("{}{}", coinbase_api::BASE_API_URL, request_path), ReqType::GET)
            .add_header("accept", "application/json")
            .add_header("CB-ACCESS-KEY", m_api_key)
            .add_header("CB-ACCESS-TIMESTAMP", std::to_string(time_seconds))
            .add_header("CB-ACCESS-SIGN", signature);

        std::vector<CURLcode> statues;
        size_t failed = req.fetch_all(statues);
        if (failed > 0)
        {
            log("ERROR: get_account to {} failed with {}", 
                    exchange_api::to_string(coinbase_api::exchange_api_id),
                    req.get_error_msg(0, statues[0]));

            return std::nullopt;
        }

        Document doc;
        std::string response {req.get_response(0)};
        rapidjson::ParseResult res {doc.Parse(response.c_str())};
        if (!res)
        {
            log("ERROR get_account \"{}\", parse error at offet {}: to parse: {}", response,
                    res.Offset(), rapidjson::GetParseError_En(res.Code()));
            return std::nullopt;
        }

        if (doc.HasMember("error"))
        {
            log("ERROR get_account \"{}\"", response);
            return std::nullopt;
        }

        if (!doc.HasMember("account"))
        {
            log("ERROR get_order unkown response \"{}\"", response);
            return std::nullopt;
        }
        const Value& account = doc["account"];
        log("account {}: name = {}, type = {}, available =  ({}, {}), hold = ({}, {})",
                get_json_string(account, "uuid").value_or("<n/a>"),
                get_json_string(account, "name").value_or("<n/a>"),
                get_json_string(account, "type").value_or("<n/a>"),
                get_json_string(account, "available_balance", "currency").value_or("<n/a>"),
                get_json_string(account, "available_balance", "value").value_or("<n/a>"),
                get_json_string(account, "hold", "currency").value_or("<n/a>"),
                get_json_string(account, "hold", "value").value_or("<n/a>")
           );

        return std::optional<Document>(std::move(doc));
    }

    bool cancel_limit_order(const std::string& order_id, size_t attempts = 2)
    {
        for (size_t i = 0; i < attempts; ++i)
        {
            cancel_order_code code = try_cancel_limit_order(order_id);
            if (code == cancel_order_code::OK)
                return true;
            if (code == cancel_order_code::UNKNOWN_ORDER)
            {
                log("cancel order {} failed ({:d}) with UNKOWN_ORDER", order_id, i);
                continue;
            }
            break;
        }

        return false;
    }


    bool create_limit_sell_order(instrument_pair_t pair, double limit_price, double quantity, std::string& order_id_ref)
    {
        requests_t req;
        requests_t::statuses_t statuses;
        create_limit_order_request(req, "SELL", pair, limit_price, quantity);
        req.fetch_all(statuses);
        return parse_create_limit_order_request(req, statuses, 0, order_id_ref);
    }


    bool create_limit_buy_order(instrument_pair_t pair, double limit_price, double quantity, std::string& order_id_ref)
    {
        requests_t req;
        requests_t::statuses_t statuses;
        create_limit_order_request(req, "BUY", pair, limit_price, quantity);
        req.fetch_all(statuses);

        return parse_create_limit_order_request(req, statuses, 0, order_id_ref);
    }



    std::optional<order_status> get_order(const std::string& order_id, Document& doc)
    {
        const std::string request_path{std::format("{}/{}", coinbase_api::GET_ORDER_PATH, order_id)};

        long time_seconds {std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch().count()};
        std::string signature {sign_payload(request_path, "", time_seconds, "GET")};

        requests_t req;
        req.add_request(std::format("{}{}", coinbase_api::BASE_API_URL, request_path), ReqType::GET)
            .add_header("accept", "application/json")
            .add_header("CB-ACCESS-KEY", m_api_key)
            .add_header("CB-ACCESS-TIMESTAMP", std::to_string(time_seconds))
            .add_header("CB-ACCESS-SIGN", signature);

        std::vector<CURLcode> statues;
        size_t failed = req.fetch_all(statues);
        if (failed > 0)
        {
            log("ERROR: get_order to {} failed with {}", 
                    exchange_api::to_string(coinbase_api::exchange_api_id),
                    req.get_error_msg(0, statues[0]));

            return std::nullopt;
        }

        std::string response {req.get_response(0)};
        rapidjson::ParseResult res {doc.Parse(response.c_str())};
        if (!res)
        {
            log("ERROR get_order \"{}\", parse error at offet {}: to parse: {}", response,
                    res.Offset(), rapidjson::GetParseError_En(res.Code()));
            return std::nullopt;
        }

        if (doc.HasMember("error"))
        {
            log("ERROR get_order \"{}\"", response);
            return std::nullopt;
        }

        if (!doc.HasMember("order"))
        {
            log("ERROR get_order unkown response \"{}\"", response);
            return std::nullopt;
        }
        const Value& order = doc["order"];
        log("{} order: user_id = {}, status = {}, time_in_force = {}, total_fees = {}, pending_cancel = {}",
                order["side"].GetString(),
                order["user_id"].GetString(),
                order["status"].GetString(),
                order["time_in_force"].GetString(),
                order["total_fees"].GetString(),
                std::to_string(order["pending_cancel"].GetBool()));

        return std::optional<order_status>(order_status{order_id,
                    order_status::side_from_string(order["side"].GetString()),
                    order_status::status_from_string(order["status"].GetString())});
    }


    struct account_info
    {
        std::string uuid;
        bool is_fiat;
        std::string currency;
        double balance;

        account_info(const std::string& uuid, bool is_fiat, std::string currency, double balance)
            : uuid(uuid), is_fiat(is_fiat), currency(currency), balance(balance)
        {}
    };

    std::optional<std::vector<account_info>> list_accounts(bool verbose=false)
    {
        long time_seconds {std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch().count()};
        std::string signature {sign_payload(coinbase_api::LIST_ACCOUNTS_PATH, "", time_seconds, "GET")};

        requests_t req;
        req.add_request(std::format("{}{}", coinbase_api::BASE_API_URL, coinbase_api::LIST_ACCOUNTS_PATH), ReqType::GET)
            .add_header("accept", "application/json")
            .add_header("CB-ACCESS-KEY", m_api_key)
            .add_header("CB-ACCESS-TIMESTAMP", std::to_string(time_seconds))
            .add_header("CB-ACCESS-SIGN", signature);

        std::vector<CURLcode> statues;
        size_t failed = req.fetch_all(statues);
        if (failed > 0)
        {
            log("ERROR: list_account to {} failed with {}", 
                    exchange_api::to_string(coinbase_api::exchange_api_id),
                    req.get_error_msg(0, statues[0]));

            return std::nullopt;
        }

        Document doc;
        std::string response {req.get_response(0)};
        rapidjson::ParseResult res {doc.Parse(response.c_str())};
        if (!res)
        {
            log("ERROR list_account \"{}\", parse error at offet {}: to parse: {}", response,
                    res.Offset(), rapidjson::GetParseError_En(res.Code()));
            return std::nullopt;
        }

        if (doc.HasMember("error"))
        {
            log("ERROR list_account \"{}\"", response);
            return std::nullopt;
        }

        if (!doc.HasMember("accounts") || !doc["accounts"].IsArray())
        {
            log("ERROR list_account unkown response \"{}\"", response);
            return std::nullopt;
        }
        const Value::Array& accounts = doc["accounts"].GetArray();

        std::vector<account_info> infos;
        infos.reserve(accounts.Size());

        for(size_t i = 0; i < accounts.Size(); ++i)
        {
            const Value& account = accounts[i];
            auto op_uuid {get_json_string(account, "uuid")};
            auto op_type {get_json_string(account, "type")};
            auto op_currency {get_json_string(account, "available_balance", "currency")};
            auto op_value  {get_json_string(account, "available_balance", "value")};
            if (op_uuid && op_type && op_currency && op_value)
            {
                bool is_fiat = (op_type.value() == "ACCOUNT_TYPE_FIAT");
                double value = std::stod(op_value.value());
                infos.push_back(account_info(op_uuid.value(), is_fiat, op_currency.value(), value));
            }


            if(verbose)
            {
                log("account {}: name = {}, type = {}, available =  ({}, {}), hold = ({}, {})",
                    op_uuid.value_or("<n/a>"),
                    get_json_string(account, "name").value_or("<n/a>"),
                    get_json_string(account, "type").value_or("<n/a>"),
                    get_json_string(account, "available_balance", "currency").value_or("<n/a>"),
                    get_json_string(account, "available_balance", "value").value_or("<n/a>"),
                    get_json_string(account, "hold", "currency").value_or("<n/a>"),
                    get_json_string(account, "hold", "value").value_or("<n/a>"));
            }
        }

        return std::optional<std::vector<account_info>>(std::move(infos));
    }


    std::optional<double> get_fiat_account_balance(std::string currency = "USD", bool refetch = false)
    {
        thread_local std::optional<std::vector<account_info>> accounts {list_accounts()};

        if (refetch)
        {
            accounts = list_accounts();
        }

        if (!accounts.has_value())
            return std::nullopt;


        for (const account_info& ac : accounts.value())
        {
            if (ac.is_fiat && ac.currency == currency)
            {
                return ac.balance;
            }
        }

        return std::nullopt;
    }

};

#endif
