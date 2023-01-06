
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")

add_test_executable("test-requests" "test_requests.cpp" "requests.cpp")

add_test_executable("test-arbritrage-trader" "test_trader.cpp" "exchange_api.cpp;crypto.cpp;json.cpp;requests.cpp")
