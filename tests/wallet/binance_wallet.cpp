#include "config.h"

// disable verbose log
#undef WEBSOCKET_LOGS
#undef MESSAGE_PAYLOAD_LOG
#undef VERBOSE_CURL_REQUESTS

#include "wallet_binance.h"

#include <string>
#include <cmath>



/**
 * Submits a sell order and buy order to coinbase using wallet<coinbase_api> type
 */
int main(int argc, char** argv)
{
    if (argc != 3)
    {
        log("test-binance-wallet <price> <dollars to spend>");
        return 1;
    }

    double price = std::stod(argv[1]);
    double spend = std::stod(argv[2]);

    const double quantity = spend/price;

    instrument_pair_t pair {instrument("ETH"), instrument("USD")};
    wallet<binance_api> w {pair,
        "EiWengYXXXXXXXXXXXXXXXXXXXXXXHRd53UfEeckVIJpTSqDJYEiuPtviE6pVKU9",
        "F3qkDsWXXXXXXXXXXXXXXXXXXXXXXXXXJIwUTNrZ4cI8mWuUdK2LxvRREzE6gxtR"};

    log("--- getting balance ---");
    log("USD BALANCE: {}", w.get_asset_account_balance("USD").value_or(std::nan("")));

    // buy order
    log("\n--- buying order ---");
    std::optional<order_status> op_status{w.create_limit_buy_order(pair, price, quantity)};
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
    op_status = w.create_limit_sell_order(pair, price, quantity);
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


