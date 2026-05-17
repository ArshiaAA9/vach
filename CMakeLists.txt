cmake_minimum_required(VERSION 3.16)
project(vach VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # Critical for clangd
set(SOURCE_DIR "${CMAKE_SOURCE_DIR}/src")


find_package(Boost REQUIRED)

add_subdirectory(whisper.cpp)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "${SOURCE_DIR}/*.cpp")

add_executable(vach
    ${SOURCES}
)

target_link_libraries(vach PRIVATE
    Boost::headers
    whisper
    pthread
)

target_include_directories(vach PRIVATE
    ${CMAKE_SOURCE_DIR}/whisper.cpp/include  # ← this is what was missing
)

target_compile_options(vach PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)
