cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
set(CMAKE_CXX_STANDARD 26)

project(rest_client_2)
set(PROJECT_VERSION 1.0.0.0)
project(${PROJECT_NAME} VERSION ${PROJECT_VERSION} LANGUAGES CXX)

add_compile_options(-c -Wall -O3 -std=c++26)

add_executable(${PROJECT_NAME}
        main.cpp
        rest_client.cpp
        rest_client.hpp
)

target_include_directories(${PROJECT_NAME} PUBLIC ${THIRD_PARTY_DIR}/json/include)
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
