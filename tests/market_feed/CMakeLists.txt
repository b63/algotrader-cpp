
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests/market_feed")

add_test_executable("test-coinbase" "coinbase_feed.cpp" "exchange_api.cpp;crypto.cpp;json.cpp")

add_test_executable("test-binance" "binance_feed.cpp" "exchange_api.cpp;crypto.cpp;json.cpp;requests.cpp")

add_test_executable("test-orderbook" "exchange_orderbooks.cpp" "exchange_api.cpp;crypto.cpp;json.cpp;requests.cpp")

