cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
set(CMAKE_CXX_STANDARD 26)

project(rest_client_2)
set(PROJECT_VERSION 1.0.0.0)
project(${PROJECT_NAME} VERSION ${PROJECT_VERSION} LANGUAGES CXX)

add_compile_options(-c -Wall -O3 -std=c++26)

include(FetchContent)

message(STATUS "[dependency] fetching Boost")
set(BOOST_ENABLE_PYTHON OFF)
set(BOOST_IOSTREAMS_ENABLE_ZLIB ON)
FetchContent_Declare(boost
        GIT_REPOSITORY https://github.com/boostorg/boost.git
        GIT_TAG boost-1.85.0
        GIT_SHALLOW ON
        GIT_PROGRESS ON
        GIT_SUBMODULES ${BOOST_REQD_SUBMODULES}
        CMAKE_ARGS  -DBOOST_IOSTREAMS_ENABLE_ZLIB=ON
        OVERRIDE_FIND_PACKAGE
)
FetchContent_MakeAvailable(boost)

find_package(OpenSSL REQUIRED)

include_directories(${THIRD_PARTY_DIR}/json/include)


add_executable(${PROJECT_NAME}
        main.cpp
        rest_client.cpp
        rest_client.hpp
)

target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_SOURCE_DIR}/configuration")
target_link_directories(${PROJECT_NAME} PUBLIC "${CMAKE_BINARY_DIR}/configuration")

target_link_libraries(${PROJECT_NAME}
        config
        pthread
        Boost::system
        OpenSSL::SSL
        OpenSSL::Crypto
        ${EXTRA_LIBS}
)
