#include "exchange_api.h"

std::string instrument_pair::to_coinbase(instrument_pair_t pair)
{
    return pair.first.name() + "-" + pair.second.name();;
}

std::string instrument_pair::to_binance(instrument_pair_t pair)
{
    return pair.first.name() + pair.second.name();;
}

std::string instrument_pair::to_binance_lower(instrument_pair_t pair)
{
    return pair.first.name_lower() + pair.second.name_lower();;
}
