set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests/wallet")

add_test_executable("test-coinbase-wallet" "coinbase_wallet.cpp" "exchange_api.cpp;crypto.cpp;json.cpp")

