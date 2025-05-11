project(btree_index_project)


add_library(${PROJECT_NAME} STATIC 
    btree_page.cpp
)

include_directories(
    ${PROJECT_SOURCE_DIR}
)
