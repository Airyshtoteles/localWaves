cmake_minimum_required(VERSION 3.16)

project(CppVideoLan VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(SOURCES
    src/main.cpp
    src/gui/MainWindow.cpp
    src/gui/MainWindow.hpp
    src/server/HttpServer.cpp
    src/server/HttpServer.hpp
    src/server/HttpConnection.cpp
    src/server/HttpConnection.hpp
    src/server/MimeTypes.hpp
    src/utils/NetworkUtils.hpp
)

add_executable(CppVideoLan WIN32 ${SOURCES})

target_link_libraries(CppVideoLan PRIVATE Qt6::Widgets Qt6::Network)

if(WIN32)
    # add_compile_definitions(_WIN32_WINNT=0x0601) # Commented out to avoid redefinition warning
    target_link_libraries(CppVideoLan PRIVATE ws2_32 mswsock)
endif()

enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(TestLocalWaves tests/TestLocalWaves.cpp)
target_link_libraries(TestLocalWaves PRIVATE Qt6::Test Qt6::Network)
add_test(NAME LocalWavesTest COMMAND TestLocalWaves)
