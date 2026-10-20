cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
set(CMAKE_CXX_STANDARD 26)

project(binance)
set(PROJECT_VERSION 1.0.0.0)
project(${PROJECT_NAME} VERSION ${PROJECT_VERSION} LANGUAGES CXX)

add_compile_options(-c -Wall -O3 -std=c++26)

add_executable(${PROJECT_NAME}
        main.cpp
        binance_web_socket_client.cpp
        binance_web_socket_client.hpp
        market_update.hpp
        binance_types.hpp
        binance_market_data_parser.hpp
        binance_market_data_parser.cpp
        binance_rest_client.hpp
        binance_rest_client.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC ${THIRD_PARTY_DIR}/json/include)

target_include_directories(${PROJECT_NAME} PUBLIC ${THIRD_PARTY_DIR}/simdjson/include)
target_link_directories(${PROJECT_NAME} PUBLIC ${THIRD_PARTY_DIR}/simdjson/build)

target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_SOURCE_DIR}/configuration")
target_link_directories(${PROJECT_NAME} PUBLIC "${CMAKE_BINARY_DIR}/configuration")

target_link_libraries(${PROJECT_NAME}
        config
        simdjson
        pthread
        Boost::asio
        Boost::beast
        Boost::json
        crypto
        ssl
        ${EXTRA_LIBS}
)
