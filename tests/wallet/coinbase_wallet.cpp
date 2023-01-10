#include "config.h"
#include "rapidjson/encodedstream.h"
// disable verbose log
#undef WEBSOCKET_LOGS
#undef MESSAGE_PAYLOAD_LOG
#undef VERBOSE_CURL_REQUESTS

#include "wallet.h"

#include <string>
#include <cmath>



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
    std::string user_id {"081394e0-527e-5be0-82e7-44057921fb4a"};
    Document doc;

    log("--- listing accounts ---");
    std::vector<std::string> uuids;
    auto op_vec = w.list_accounts(true);
    if (!op_vec)
    {
        log("failed to list accounts");
    }
    log("USD BALANCE: {}", w.get_fiat_account_balance("USD", true).value_or(std::nan("")));

    // buy order
    log("\n--- buying order ---");
    if(!w.create_limit_buy_order(pair, price, QUANTITY, order_id))
    {
        log("create_immediate_buy_order FAILED");
        return 1;
    }

    log("\n--- getting order info --");
    log("USD BALANCE: {}", w.get_fiat_account_balance("USD", true).value_or(std::nan("")));
    // get information about the order
    if(!w.get_order(order_id, doc))
    {
        log("failed to get order info for {}", order_id);
    }

    // get the account information
    log("\n--- getting account info --");
    auto op_doc = w.get_account(user_id);
    if (!op_doc)
    {
        log("failed to get account info {}", user_id);
    }
    else
    {
        log("account info: {}", to_string(op_doc.value()));
    }

    log("\n--- cancelling order ---");
    // try to cancel it
    if (!w.cancel_limit_order(order_id, 1))
    {
        log("cancel_order FAILED");
    }

    log("\n--- getting order info --");
    // get information about it again
    if(!w.get_order(order_id, doc))
    {
        log("failed to get order info for {}", order_id);
    }


    log("\n--- selling order --");
    // create a sell order
    if(!w.create_limit_sell_order(pair, price, QUANTITY, order_id))
    {
        log("create_immediate_sell_order FAILED");
        return 1;
    }

    log("\n--- getting order info --");
    log("USD BALANCE: {}", w.get_fiat_account_balance().value_or(std::nan("")));
    // get information about it
    if(!w.get_order(order_id, doc))
    {
        log("failed to get order info for {}", order_id);
    }

    // get the account information again
    log("\n--- getting account info --");
    op_doc = w.get_account(user_id);
    if (!op_doc)
    {
        log("failed to get account info {}", user_id);
    }
    else
    {
        log("account info: {}", to_string(op_doc.value()));
    }
}


