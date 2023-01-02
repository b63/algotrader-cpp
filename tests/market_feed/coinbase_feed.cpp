
#include "coinbase_feed.h"
#include <iostream>


int main(void) {
    std::vector<instrument_pair_t> pairs;
    pairs.emplace_back(instrument("ETH"), instrument("USD"));

    market_feed<coinbase_api> feed (pairs, "OVvF5YREDjPdLz9J", "gxDeuHUXte1vbuVRdRy3dHLhXgO0M6ej");
    feed.start_feed();

    using namespace std::chrono_literals;
    std::this_thread::sleep_for(5s);

    feed.close();
    feed.join();
}
