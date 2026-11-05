cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
set(CMAKE_CXX_STANDARD 26)

project(binance_order_execution_module)
set(PROJECT_VERSION 1.0.0.0)
project(${PROJECT_NAME} VERSION ${PROJECT_VERSION} LANGUAGES CXX)

add_compile_options(-c -Wall -O3 -std=c++26)

add_executable(${PROJECT_NAME}
        main.cpp

        networking/rest_client.cpp
        networking/rest_client.hpp
        networking/web_socket_client.cpp
        networking/web_socket_client.hpp

        common/scaled_value.hpp
        common/price.hpp
        common/quantity.hpp
        common/instrument.hpp
        common/timestamp.hpp
        common/side.hpp
        common/side.cpp

        binance/order.hpp
        binance/order.cpp

        utilities/utilities.hpp
        utilities/utilities.cpp
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
        crypto
        ssl
        ${EXTRA_LIBS}
)
