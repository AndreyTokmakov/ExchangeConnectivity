cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

project(configuration)
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
set(PROJECT_VERSION 1.0.0.0)
project(${PROJECT_NAME} VERSION ${PROJECT_VERSION} LANGUAGES CXX)

add_compile_options(-c -Wall -Wextra -O3 -std=c++26)

include_directories(${THIRD_PARTY_DIR}/json/include)

add_library(config
    configuration.hpp
    configuration_parser.hpp
    configuration_parser.cpp
)

add_executable(${PROJECT_NAME}
    main.cpp
)

target_link_libraries(${PROJECT_NAME}
    config
    ${EXTRA_LIBS}
)