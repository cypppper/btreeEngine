project(graphviz_project)

add_library(${PROJECT_NAME} STATIC 
    graphviz.cpp
)

include_directories(
    ${PROJECT_SOURCE_DIR}
)
