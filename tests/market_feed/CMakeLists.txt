
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests/market_feed")


set(coinbase_source_list exchange_api.cpp crypto.cpp json.cpp)
LIST(TRANSFORM coinbase_source_list PREPEND "${CMAKE_SOURCE_DIR}/src/")

add_executable(test-coinbase coinbase_feed.cpp ${coinbase_source_list})
target_link_options(test-coinbase PRIVATE ${OPENSSL_LIBRARIES})
target_link_libraries(test-coinbase pthread ${CURL_LIBRARIES})


set(binance_source_list exchange_api.cpp crypto.cpp json.cpp requests.cpp)
LIST(TRANSFORM binance_source_list PREPEND "${CMAKE_SOURCE_DIR}/src/")

add_executable(test-binance binance_feed.cpp ${binance_source_list})
target_link_options(test-binance PRIVATE ${OPENSSL_LIBRARIES})
target_link_libraries(test-binance pthread ${CURL_LIBRARIES})
