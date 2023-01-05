#include "binance_feed.h"
#include <iostream>


int main(void) {
    std::vector<instrument_pair_t> pairs;
    pairs.emplace_back(instrument("ETH"), instrument("USD"));

    market_feed<binance_api> feed (pairs,
            "bD9QfIu4FBdRJpviWI075M6KMX2lb9oUyLfC2IknlE4vcIbnFKQaeSm8f0vLW8te",
            "AfqGK6Jf8HQGiI93RC7jYDJMKVS9cMlc4adhvcXeMSOSUKQEIkmIV9SmeZDu0kd5");
    feed.start_feed();

    using namespace std::chrono_literals;
    std::this_thread::sleep_for(5s);

    feed.close();
    feed.join();
}
