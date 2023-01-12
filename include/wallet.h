#ifndef _WALLET_H
#define _WALLET_H

#include "exchange_api.h"
#include "requests.h"
#include "json.h"
#include "crypto.h"

#include <chrono>
#include <ctime>
#include <optional>
#include <cstring>

template <typename ExchangeAPI>
    requires is_exchange_api<ExchangeAPI>
class wallet {};

template <typename T>
concept is_wallet = is_any<T, wallet<coinbase_api>, wallet<binance_api>>;

enum class SIDE: int {
    BUY,
    SELL,
    UNKNOWN
};


enum class STATUS: int {
    FILLED,
    OPEN,
    CANCELLED,
    FAILED,
    UNKNOWN,

};

struct order_status
{
    order_status(const std::string& order_id, SIDE side, STATUS status)
        : order_id (order_id), side(side), status(status)
    {};

    std::string order_id;
    SIDE side;
    STATUS status;

    static std::string side_to_string(SIDE side)
    {
        switch(side)
        {
            case SIDE::BUY:
                return "BUY";
            case SIDE::SELL:
                return "SELL";
            case SIDE::UNKNOWN:
                return "UNKNOWN";
        }
    }

    static SIDE side_from_string(const std::string& str)
    {
        if (!str.compare("BUY"))
            return SIDE::BUY;
        if (!str.compare("SELL"))
            return SIDE::SELL;
        return SIDE::UNKNOWN;
    }

    static STATUS status_from_string(const std::string& str)
    {
        if (!str.compare("FILLED"))
            return STATUS::FILLED;
        if (!str.compare("OPEN"))
            return STATUS::OPEN;
        if (!str.compare("CANCELLED"))
            return STATUS::CANCELLED;
        if (!str.compare("FAILED"))
            return STATUS::FAILED;
        return STATUS::UNKNOWN;
    }
};



#endif
