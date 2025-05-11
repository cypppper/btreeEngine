add_library(fmt_project STATIC
    fmt/format.cc
)

target_include_directories(fmt_project PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
