#include "config.h"
// disable verbose log
#undef WEBSOCKET_LOGS
#undef MESSAGE_PAYLOAD_LOG
#undef VERBOSE_CURL_REQUESTS

#include "wallet.h"

#include <string>



/**
 * Submits a sell order and buy order to coinbase using wallet<coinbase_api> type
 */
int main(int argc, char** argv)
{
    if (argc != 2)
    {
        log("test-coinbase-wallet <price>");
        return 1;
    }

    double price = std::stod(argv[1]);

    const double QUANTITY = 1/price;

    wallet<coinbase_api> w {"Pa64Af8wXOV8GVQc", "E5ObghoQazarFycSTKXRWRrY0FpTeTR8"};
    instrument_pair_t pair {instrument("ETH"), instrument("USD")};

    std::string order_id;
    Document doc;

    // buy order
    if(!w.create_limit_buy_order(pair, price, QUANTITY, order_id))
    {
        log("create_immediate_buy_order FAILED");
        return 1;
    }

    // get information about the order
    if(!w.get_order(order_id, doc))
    {
        log("failed to get order info for {}", order_id);
    }

    // try to cancel it
    if (!w.cancel_limit_order(order_id, 1))
    {
        log("cancel_order FAILED");
    }

    // get information about it again
    if(!w.get_order(order_id, doc))
    {
        log("failed to get order info for {}", order_id);
    }



    // create a sell order
    if(!w.create_limit_sell_order(pair, price, QUANTITY, order_id))
    {
        log("create_immediate_sell_order FAILED");
        return 1;
    }

    // get information about it
    if(!w.get_order(order_id, doc))
    {
        log("failed to get order info for {}", order_id);
    }
}


