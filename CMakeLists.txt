cmake_minimum_required(VERSION 3.23)
project("algo_trader" CXX)

set(CMAKE_CXX_COMPILER clang++) 
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-Wall -Werror -O2 -flto -stdlib=libstdc++")

set(CMAKE_BINARY_DIR "${CMAKE_SOURCE_DIR}/build")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

include_directories(BEFORE ${CMAKE_SOURCE_DIR}/include)
include_directories(BEFORE ${CMAKE_SOURCE_DIR}/asio/asio/include)
include_directories(BEFORE ${CMAKE_SOURCE_DIR}/websocketpp)

find_package(CURL COMPONENTS HTTPS REQUIRED)
list(TRANSFORM CURL_LIBRARIES REPLACE "\.so$" ".a" OUTPUT_VARIABLE CURL_LIBRARIES_STATIC)
include_directories(${CURL_INCLUDE_DIR})
message("CURL Libraries: ${CURL_LIBRARIES}")

find_package(OpenSSL COMPONENTS SSL Crypto REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

add_subdirectory(${CMAKE_SOURCE_DIR}/src)
