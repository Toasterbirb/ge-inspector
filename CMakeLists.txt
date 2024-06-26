cmake_minimum_required(VERSION 3.12)

project(ge-inspector)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
endif(CCACHE_FOUND)

add_subdirectory(vendor/clipp)
include_directories(vendor/clipp/include)

add_subdirectory(vendor/json)
include_directories(vendor/json/include)

include_directories(./include ./vendor)
file(GLOB SRC_FILES ./src/*)
add_executable(${PROJECT_NAME} ${SRC_FILES})
target_link_libraries(${PROJECT_NAME} clipp curl)

install(TARGETS ${PROJECT_NAME})
