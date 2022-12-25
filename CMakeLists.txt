cmake_minimum_required(VERSION 3.23)
project("algo_trader" CXX)

set(CMAKE_CXX_COMPILER clang++) 
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_BINARY_DIR "${CMAKE_SOURCE_DIR}/build")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

include_directories(BEFORE ${CMAKE_SOURCE_DIR}/include)
include_directories(BEFORE ${CMAKE_SOURCE_DIR}/asio/asio/include)
include_directories(BEFORE ${CMAKE_SOURCE_DIR}/websocketpp)

find_package(CURL COMPONENTS HTTPS REQUIRED)
list(TRANSFORM CURL_LIBRARIES REPLACE "\.so$" ".a" OUTPUT_VARIABLE CURL_LIBRARIES_STATIC)
include_directories(${CURL_INCLUDE_DIR})
message(STATUS "CURL_LIBRARIES = ${CURL_LIBRARIES}")
message(STATUS "CURL_LIBRARIES_STATIC = ${CURL_LIBRARIES_STATIC}")

if (APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
    set(OPENSSL_ROOT_DIR "/usr/local/homebrew/Cellar/openssl@3/3.0.7")
    set(OPENSSL_LIBRARIES "${OPENSSL_ROOT_DIR}/lib/libssl.dylib;${OPENSSL_ROOT_DIR}/lib/libcrypto.dylib")
    set(OPENSSL_INCLUDE_DIR "${OPENSSL_ROOT_DIR}/include")
    message(STATUS "OPENSSL_LIBRARIES = ${OPENSSL_LIBRARIES}")
    set(CXX_STD_LIB "libc++")
else()
    find_package(OpenSSL COMPONENTS SSL Crypto REQUIRED)
    set(CXX_STD_LIB "libstdc++")
endif()


message(STATUS "OPENSSL_INCLUDE_DIR = ${OPENSSL_INCLUDE_DIR}")
include_directories(${OPENSSL_INCLUDE_DIR})

set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-Wall -Werror -O2 -flto -stdlib=${CXX_STD_LIB}")

add_subdirectory(${CMAKE_SOURCE_DIR}/src)
