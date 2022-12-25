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
message(STATUS "CURL_LIBRARIES = ${CURL_LIBRARIES}")
message(STATUS "CURL_LIBRARIES_STATIC = ${CURL_LIBRARIES_STATIC}")

if (APPLE)
    set(OPENSSL_ROOT_DIR "/usr/local/homebrew/Cellar/openssl@3/3.0.7")

    # set(OPENSSL_PKGCONFIG_DIR "/usr/local/homebrew/Cellar/openssl@3/3.0.7/lib/pkgconfig")
    # find_path(OPENSSL_PKGCONFIG_DIR 
    #                 NAMES "lib/pkgconfig/openssl.pc"
    #                 PATHS ${OPENSSL_ROOT_DIR}
    #                 NO_DEFAULT_PATH
    #                 NO_PACKAGE_ROOT_PATH
    #                 NO_CMAKE_PATH
    #                 NO_CMAKE_ENVIRONMENT_PATH
    #                 NO_SYSTEM_ENVIRONMENT_PATH
    #                 NO_CMAKE_SYSTEM_PATH
    #                 NO_CMAKE_INSTALL_PREFIX)

    # message(STATUS "OPENSSL_PKGCONFIG_DIR = ${OPENSSL_PKGCONFIG_DIR}")
    # message(STATUS "OPENSSL_ROOT_DIR = ${OPENSSL_ROOT_DIR}")

    # set(ENV{PKG_CONFIG_PATH} "${OPENSSL_PKGCONFIG_DIR}:$ENV{PKG_CONFIG_PATH}")
    # find_package(OpenSSL COMPONENTS SSL Crypto REQUIRED)

    set(OPENSSL_LIBRARIES "${OPENSSL_ROOT_DIR}/lib/libssl.dylib;${OPENSSL_ROOT_DIR}/lib/libcrypto.dylib")
    set(OPENSSL_INCLUDE_DIR "${OPENSSL_ROOT_DIR/include}")
else()
    find_package(OpenSSL COMPONENTS SSL Crypto REQUIRED)
endif()

include_directories(${OPENSSL_INCLUDE_DIR})

message(STATUS "OPENSSL_LIBRARIES = ${OPENSSL_LIBRARIES}")
add_subdirectory(${CMAKE_SOURCE_DIR}/src)
