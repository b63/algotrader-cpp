
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests/market_feed")

function(add_test_executable name test_source sources)
    LIST(TRANSFORM sources PREPEND "${CMAKE_SOURCE_DIR}/src/")

    add_executable(${name} ${test_source} ${sources})
    target_link_options(${name} PRIVATE ${OPENSSL_LIBRARIES})
    target_link_libraries(${name} pthread ${CURL_LIBRARIES})
endfunction()

add_test_executable("test-coinbase" "coinbase_feed.cpp" "exchange_api.cpp;crypto.cpp;json.cpp")

add_test_executable("test-binance" "binance_feed.cpp" "exchange_api.cpp;crypto.cpp;json.cpp;requests.cpp")

add_test_executable("test-coinbase-orderbook" "coinbase_orderbook.cpp" "exchange_api.cpp;crypto.cpp;json.cpp;requests.cpp")
