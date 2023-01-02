
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests/market_feed")

set(source_list exchange_api.cpp crypto.cpp json.cpp)
LIST(TRANSFORM source_list PREPEND "${CMAKE_SOURCE_DIR}/src/")
add_executable(test-coinbase coinbase_feed.cpp ${source_list})
target_link_options(test-coinbase PRIVATE ${OPENSSL_LIBRARIES})
target_link_libraries(test-coinbase pthread)
