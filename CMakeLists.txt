cmake_minimum_required(VERSION 3.0)
project(btree_engine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20")
add_definitions("-Wall -g")

# 设置变量，下面的代码都可以用到
set(BTREE_INDEX_DIR ${PROJECT_SOURCE_DIR}/src/btree_index)
set(STATUS_DIR ${PROJECT_SOURCE_DIR}/src/status)



# 规定.h头文件路径
include_directories(
    ${PROJECT_SOURCE_DIR}
    ${BTREE_INDEX_DIR}
    ${STATUS_DIR}
)

# 编译子文件夹的CMakeLists.txt
add_subdirectory(${BTREE_INDEX_DIR})
add_subdirectory(${STATUS_DIR})

# 生成可执行文件
add_executable(${PROJECT_NAME} main.cpp )

# 链接操作
target_link_libraries(
    ${PROJECT_NAME}
    btree_index_project
    status_project
)
