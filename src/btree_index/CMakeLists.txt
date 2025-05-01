project(btree_index_project)


add_library(${PROJECT_NAME} STATIC 
    index.cpp
    inner_page.cpp
    leaf_page.cpp
    btree_page.cpp
)

include_directories(
    ${PROJECT_SOURCE_DIR}
)
