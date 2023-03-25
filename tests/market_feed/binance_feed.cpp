#include "binance_feed.h"
#include <iostream>

// verbose log
#define WEBSOCKET_LOGS
#define MESSAGE_PAYLOAD_LOG

int main(void) {
    std::vector<instrument_pair_t> pairs;
    pairs.emplace_back(instrument("ETH"), instrument("USD"));

    market_feed<binance_api> feed (pairs,
            "bD9QfIuXXXXXXXXXXXXXXXXXXX2lb9oUyLfC2IknlE4vcIbnFKQaeSm8f0vLW8te",
            "AfqGK6JXXXXXXXXXXXXXXXXXXXXXXMlc4adhvcXeMSOSUKQEIkmIV9SmeZDu0kd5");
    feed.start_feed();

    using namespace std::chrono_literals;
    std::this_thread::sleep_for(5s);

    feed.close();
    feed.join();
}
