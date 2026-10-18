cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
set(CMAKE_CXX_STANDARD 26)

project(rest_client_async)
set(PROJECT_VERSION 1.0.0.0)
project(${PROJECT_NAME} VERSION ${PROJECT_VERSION} LANGUAGES CXX)

add_compile_options(-c -Wall -O3 -std=c++26)

include(FetchContent)

message(STATUS "[dependency] fetching boost")
set(BOOST_ENABLE_PYTHON OFF)
set(BOOST_IOSTREAMS_ENABLE_ZLIB ON)
FetchContent_Declare(boost
        GIT_REPOSITORY https://github.com/boostorg/boost.git
        GIT_TAG boost-1.86.0
        GIT_SHALLOW ON
        GIT_PROGRESS ON
        CMAKE_ARGS  -DBOOST_IOSTREAMS_ENABLE_ZLIB=ON
)
FetchContent_MakeAvailable(boost)

message(STATUS "[dependency] fetching OpenSSL")
FetchContent_Declare(OpenSSL
        GIT_REPOSITORY https://github.com/openssl/openssl.git
        GIT_TAG        openssl-3.3.2
        GIT_SHALLOW ON
        GIT_PROGRESS ON
        BUILD_COMMAND make
)
FetchContent_MakeAvailable(OpenSSL)

set (Boost_NO_SYSTEM_PATHS ON)

include_directories(${THIRD_PARTY_DIR}/json/include)

add_executable(${PROJECT_NAME}
        main.cpp
        rest_client_async.cpp
        rest_client_async.hpp
)

target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_SOURCE_DIR}/configuration")
target_link_directories(${PROJECT_NAME} PUBLIC "${CMAKE_BINARY_DIR}/configuration")

target_link_libraries(${PROJECT_NAME}
        config
        pthread
        Boost::asio
        Boost::beast
        Boost::json
        crypto
        ssl
        ${EXTRA_LIBS}
)
