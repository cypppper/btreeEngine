cmake_minimum_required(VERSION 3.10)
project(btree_engine)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++23")
add_definitions("-Wall -g")

set(BTREE_INDEX_DIR ${PROJECT_SOURCE_DIR}/src/btree_index)
set(STATUS_DIR ${PROJECT_SOURCE_DIR}/src/status)
set(GRAPHVIZ_DIR ${PROJECT_SOURCE_DIR}/src/graphviz)
set(FORMAT_DIR ${PROJECT_SOURCE_DIR}/src/format)

include_directories(
    ${PROJECT_SOURCE_DIR}
    ${BTREE_INDEX_DIR}
    ${STATUS_DIR}
    ${GRAPHVIZ_DIR}
    ${FORMAT_DIR}
)

add_subdirectory(${BTREE_INDEX_DIR})
add_subdirectory(${STATUS_DIR})
add_subdirectory(${GRAPHVIZ_DIR})
add_subdirectory(${FORMAT_DIR})

add_executable(unittest unittest.cpp )
add_executable(${PROJECT_NAME} engine.cpp)

target_link_libraries(
    ${PROJECT_NAME}
    btree_index_project
    status_project
    graphviz_project
    fmt_project
)

target_link_libraries(
    unittest
    btree_index_project
    status_project
    graphviz_project
    fmt_project
)

