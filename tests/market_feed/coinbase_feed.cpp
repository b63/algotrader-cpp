#include "coinbase_feed.h"
#include "config.h"
#include <iostream>

// verbose log
#define WEBSOCKET_LOGS
#define MESSAGE_PAYLOAD_LOG
#define VERBOSE_CURL_REQUESTS

int main(void) {
    std::vector<instrument_pair_t> pairs;
    pairs.emplace_back(instrument("ETH"), instrument("USD"));

    market_feed<coinbase_api> feed (pairs, "Pa64Af8wXOV8GVQc", "E5ObghoQazarFycSTKXRWRrY0FpTeTR8");
    feed.start_feed();

    using namespace std::chrono_literals;
    std::this_thread::sleep_for(5s);

    feed.close();
    feed.join();
}
