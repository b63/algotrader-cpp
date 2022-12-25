

add_executable(algo-trader main.cpp)
target_link_options(algo-trader PRIVATE ${OPENSSL_LIBRARIES})
target_link_libraries(algo-trader pthread ${CURL_LIBRARIES} )
