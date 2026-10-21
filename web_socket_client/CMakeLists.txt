cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
set(CMAKE_CXX_STANDARD 26)

project(web_socket_client)
set(PROJECT_VERSION 1.0.0.0)
project(${PROJECT_NAME} VERSION ${PROJECT_VERSION} LANGUAGES CXX)

add_compile_options(-c -Wall -O3 -std=c++26)

add_executable(${PROJECT_NAME}
        main.cpp
        web_socket_client.cpp
        web_socket_client.hpp
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
