cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

set(CMAKE_CXX_STANDARD 26)

project(ExchangeConnectivity)
set(PROJECT_VERSION 1.0.0.0)
project(${PROJECT_NAME} VERSION ${PROJECT_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
# add_compile_options(-c -Wall -O3 -std=c++26)
add_compile_options(-c -Wall -O3 -freflection -std=c++26)

# 5.15 - My home Ubuntu-20.04
if (CMAKE_SYSTEM_VERSION MATCHES "5.15")
    set(THIRD_PARTY_DIR /home/andtokm/DiskS/ProjectsUbuntu/third_party)
    set(EXTRA_LIBS tbb)
else()
    set(THIRD_PARTY_DIR /home/andtokm/Projects/third_party)
endif()

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

add_subdirectory(binance)
add_subdirectory(binance_connector)
add_subdirectory(binance_connector_old)
add_subdirectory(binance_data_parser)
add_subdirectory(configuration)
add_subdirectory(rest_client)
add_subdirectory(rest_client_2)
add_subdirectory(rest_client_async)
add_subdirectory(utilities)
