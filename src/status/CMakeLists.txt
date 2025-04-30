project(status_project)

add_library(${PROJECT_NAME} STATIC 
    status.cpp
)

include_directories(
    ${PROJECT_SOURCE_DIR}
)
