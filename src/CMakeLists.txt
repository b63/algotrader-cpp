

add_executable(algo-trader main.cpp)
#target_link_options(algo-trader PRIVATE -l${CURL_LIBRARIES_STATIC})
target_link_libraries(algo-trader pthread  ${CURL_LIBRARIES} )
