cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
set(CMAKE_CXX_STANDARD 26)

project(binance_data_parser)
set(PROJECT_VERSION 1.0.0.0)
project(${PROJECT_NAME} VERSION ${PROJECT_VERSION} LANGUAGES CXX)

add_compile_options(-c -Wall -O3 -std=c++26)

include_directories(${THIRD_PARTY_DIR}/simdjson/include)
link_directories(${THIRD_PARTY_DIR}/simdjson/build)

add_executable(${PROJECT_NAME}
        main.cpp
        BinanceDataParser.cpp
        BinanceDataParser.hpp
)

target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_SOURCE_DIR}/configuration")
target_link_directories(${PROJECT_NAME} PUBLIC "${CMAKE_BINARY_DIR}/configuration")

target_link_libraries(${PROJECT_NAME}
        config
        pthread
        simdjson
        ${EXTRA_LIBS}
)
