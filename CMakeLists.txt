cmake_minimum_required(VERSION 3.10)
project(InputRedirectionClient-Raylib)

file(GLOB_RECURSE SRC_FILES src/*.cpp)
add_executable(${PROJECT_NAME} ${SRC_FILES})

add_subdirectory(libs/raylib)

target_link_libraries(${PROJECT_NAME} raylib)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/raylib/src
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/raygui/src
)