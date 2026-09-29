cmake_minimum_required(VERSION 3.20)

project(gameboy_emulator C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(SDL2 REQUIRED)

add_library(gameboy_core STATIC
    src/gb.c
    src/cpu.c
    src/memory.c
    src/cartridge.c
    src/timer.c
    src/ppu.c
    src/input.c
)

target_include_directories(gameboy_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(gameboy src/main.c)
target_include_directories(gameboy PRIVATE
    ${SDL2_INCLUDE_DIRS}
)
target_link_libraries(gameboy PRIVATE
    gameboy_core
    ${SDL2_LIBRARIES}
)

include(CTest)
if(BUILD_TESTING)
    add_executable(gameboy_tests
        tests/test_core.c
    )
    target_link_libraries(gameboy_tests PRIVATE gameboy_core)
    add_test(NAME gameboy_core COMMAND gameboy_tests)
endif()