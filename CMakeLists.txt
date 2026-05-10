cmake_minimum_required(VERSION 3.14)
project(vach VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # Critical for clangd
set(SOURCE_DIR "${CMAKE_SOURCE_DIR}/src")


find_package(Boost REQUIRED)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "${SOURCE_DIR}/*.cpp")

add_executable(vach
    ${SOURCES}
)

target_link_libraries(vach PRIVATE
    Boost::headers
)

target_compile_options(vach PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)
