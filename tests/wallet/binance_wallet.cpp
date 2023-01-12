#include "config.h"

// disable verbose log
#undef WEBSOCKET_LOGS
#undef MESSAGE_PAYLOAD_LOG
//#undef VERBOSE_CURL_REQUESTS

#include "wallet_binance.h"

#include <string>
#include <cmath>



/**
 * Submits a sell order and buy order to coinbase using wallet<coinbase_api> type
 */
int main(int argc, char** argv)
{
    if (argc != 2)
    {
        log("test-binance-wallet <price>");
        return 1;
    }

    double price = std::stod(argv[1]);

    const double QUANTITY = 1/price;

    wallet<binance_api> w {"bD9QfIu4FBdRJpviWI075M6KMX2lb9oUyLfC2IknlE4vcIbnFKQaeSm8f0vLW8te", "AfqGK6Jf8HQGiI93RC7jYDJMKVS9cMlc4adhvcXeMSOSUKQEIkmIV9SmeZDu0kd5"};
    instrument_pair_t pair {instrument("ETH"), instrument("USD")};

    log("--- getting balance ---");
    log("USD BALANCE: {}", w.get_asset_account_balance("USD").value_or(std::nan("")));

    // buy order
    log("\n--- buying order ---");
    std::optional<order_status> op_status{w.create_limit_buy_order(pair, price, QUANTITY)};
    if(!op_status)
    {
        log("create_immediate_buy_order FAILED");
        return 1;
    }
    std::string order_id {op_status.value().order_id};

    log("\n--- getting order info --");
    log("USD BALANCE: {}", w.get_asset_account_balance("USD").value_or(std::nan("")));
    // get information about the order
    if(!w.get_order(pair, order_id))
    {
        log("failed to get order info for {}", order_id);
    }


    log("\n--- cancelling order ---");
    // try to cancel it
    if (!w.cancel_limit_order(order_id))
    {
        log("cancel_order FAILED");
    }

    log("\n--- getting order info --");
    // get information about it again
    if(!w.get_order(pair, order_id))
    {
        log("failed to get order info for {}", order_id);
    }


    log("\n--- selling order --");
    // create a sell order
    op_status = w.create_limit_sell_order(pair, price, QUANTITY);
    if(!op_status)
    {
        log("create_immediate_sell_order FAILED");
        return 1;
    }
    order_id = op_status.value().order_id;

    log("\n--- getting order info --");
    log("USD BALANCE: {}", w.get_asset_account_balance("USD").value_or(std::nan("")));

    // get information about it
    if(!w.get_order(pair, order_id))
    {
        log("failed to get order info for {}", order_id);
    }
}

