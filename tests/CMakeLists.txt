
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")

add_executable(test-requests test_requests.cpp $<PATH:APPEND,${CMAKE_SOURCE_DIR}/src/,requests.cpp>)
target_link_options(test-requests PRIVATE ${OPENSSL_LIBRARIES} )
target_link_libraries(test-requests pthread ${CURL_LIBRARIES})
