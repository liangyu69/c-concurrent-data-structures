cmake_minimum_required(VERSION 3.10)
project(concurrent_data_structures C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 编译选项
add_compile_options(-Wall -Wextra -pthread)

# ============ 库 ============
add_library(blocking_queue STATIC src/blocking_queue.c)
target_include_directories(blocking_queue PUBLIC include)

add_library(spsc_queue STATIC src/spsc_queue.c)
target_include_directories(spsc_queue PUBLIC include)

add_library(hash_map STATIC src/hash_map.c)
target_include_directories(hash_map PUBLIC include)

add_library(locked_hash_map STATIC src/locked_hash_map.c)
target_include_directories(locked_hash_map PUBLIC include)

add_library(striped_hash_map STATIC src/striped_hash_map.c)
target_include_directories(striped_hash_map PUBLIC include)

add_library(naive_queue STATIC src/naive_queue.c)
target_include_directories(naive_queue PUBLIC include)

# ============ 测试 ============
add_executable(test_queue tests/test_queue.c)
target_link_libraries(test_queue blocking_queue)

add_executable(test_spsc tests/test_spsc.c)
target_link_libraries(test_spsc spsc_queue)

add_executable(test_hash_map tests/test_hash_map.c)
target_link_libraries(test_hash_map hash_map)

add_executable(test_locked_hash_map tests/test_locked_hash_map.c)
target_link_libraries(test_locked_hash_map locked_hash_map)

add_executable(test_striped_hash_map tests/test_striped_hash_map.c)
target_link_libraries(test_striped_hash_map striped_hash_map locked_hash_map)

# ============ 示例 ============
add_executable(echo_server examples/echo_server.c)
target_link_libraries(echo_server blocking_queue)

add_executable(char_count examples/char_count.c)
target_link_libraries(char_count striped_hash_map)

# ============ 可选：TSan ============
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
if(ENABLE_TSAN)
    add_compile_options(-fsanitize=thread -g -O1)
    add_link_options(-fsanitize=thread)
endif()