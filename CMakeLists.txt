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
include_directories(BEFORE ${CMAKE_SOURCE_DIR}/rapidjson/include)
include_directories(BEFORE ${CMAKE_SOURCE_DIR}/fmt/include)

find_package(CURL COMPONENTS HTTPS REQUIRED)
list(TRANSFORM CURL_LIBRARIES REPLACE "\.so$" ".a" OUTPUT_VARIABLE CURL_LIBRARIES_STATIC)
include_directories(${CURL_INCLUDE_DIR})
message(STATUS "CURL_LIBRARIES = ${CURL_LIBRARIES}")
message(STATUS "CURL_LIBRARIES_STATIC = ${CURL_LIBRARIES_STATIC}")

if (NOT HOMEBREW_PATH)
    set(HOMEBREW_PATH "/opt/homebrew")
endif()

if (APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
    set(OPENSSL_ROOT_DIR "${HOMEBREW_PATH}/Cellar/openssl@3/3.0.7")
    set(OPENSSL_LIBRARIES "${OPENSSL_ROOT_DIR}/lib/libssl.dylib;${OPENSSL_ROOT_DIR}/lib/libcrypto.dylib")
    set(OPENSSL_INCLUDE_DIR "${OPENSSL_ROOT_DIR}/include")
    message(STATUS "OPENSSL_LIBRARIES = ${OPENSSL_LIBRARIES}")
    set(CXX_STD_LIB "libstdc++")
else()
    find_package(OpenSSL COMPONENTS SSL Crypto REQUIRED)
    set(CXX_STD_LIB "libstdc++")
endif()


message(STATUS "OPENSSL_INCLUDE_DIR = ${OPENSSL_INCLUDE_DIR}")
include_directories(${OPENSSL_INCLUDE_DIR})

set(COMMON_CXX_FLAGS_DEBUG "-Wall -O2 -flto -ggdb")
set(COMMON_CXX_FLAGS_RELEASE "-O3 -flto")
if (APPLE)
    # the default libstdc++ is v 13.0 which does not support all C++20 features (jthread/stop token in particular)
    # libstdc++ v 16.0 doesn't seem to either
    # see: https://libcxx.llvm.org//UsingLibcxx.html
    # use custom installation of libstdc++ instead which does support jthread
    set(LIBSTDCPP_ROOT "/opt/homebrew/Cellar/gcc/12.2.0")
    set(CUSTOM_LIBSTDCPP "-isystem ${LIBSTDCPP_ROOT}/include/c++/12 -isystem ${LIBSTDCPP_ROOT}/include/c++/12/aarch64-apple-darwin22")

    set(CMAKE_CXX_FLAGS_DEBUG   ${CMAKE_CXX_FLAGS} "${COMMON_CXX_FLAGS_DEBUG}   -nostdinc++ -nostdlib++ ${CUSTOM_LIBSTDCPP}")
    set(CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS} "${COMMON_CXX_FLAGS_RELEASE} -nostdinc++ -nostdlib++ ${CUSTOM_LIBSTDCPP}")
    set(CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS} "-L${LIBSTDCPP_ROOT}/lib/gcc/12 -Wl,-rpath,${LIBSTDCPP_ROOT}/lib/gcc/12 -lstdc++")
else()
    # for CMAKE_BUILD_TYPE=Debug
    set(CMAKE_CXX_FLAGS_DEBUG   ${CMAKE_CXX_FLAGS} "${COMMON_CXX_FLAGS_DEBUG} -stdlib=${CXX_STD_LIB}")
    # for CMAKE_BUILD_TYPE=Release
    set(CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS} "${COMMON_CXX_FLAGS_RELEASE} -stdlib=${CXX_STD_LIB}")
endif()

function(add_test_executable name test_source sources)
    LIST(TRANSFORM sources PREPEND "${CMAKE_SOURCE_DIR}/src/")

    add_executable(${name} ${test_source} ${sources})
    target_link_options(${name} PRIVATE ${OPENSSL_LIBRARIES})
    target_link_libraries(${name} pthread ${CURL_LIBRARIES})
endfunction()

add_subdirectory(${CMAKE_SOURCE_DIR}/src)
add_subdirectory(${CMAKE_SOURCE_DIR}/tests)
add_subdirectory(${CMAKE_SOURCE_DIR}/tests/market_feed)
add_subdirectory(${CMAKE_SOURCE_DIR}/tests/wallet)
