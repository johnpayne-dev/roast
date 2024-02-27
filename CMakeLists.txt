cmake_minimum_required(VERSION 3.21)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

project(roast)

set(PROJECT_FILES
    README.md
    src/main.c)

set(FLAGS
    "-Wall"
    "-Wextra"
    "-Wpedantic"
    "-Wno-unused-parameter"
    "-Wno-unused-variable"
    "-Wno-unused-function"
    "-Wno-unused-but-set-variable"
    "-Werror"
    "-std=c11")

add_executable(roast ${PROJECT_FILES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${PROJECT_FILES})

find_package(PkgConfig REQUIRED)
pkg_check_modules(DEPENDENCIES REQUIRED IMPORTED_TARGET glib-2.0)

target_link_libraries(roast PkgConfig::DEPENDENCIES)
target_compile_options(roast PRIVATE ${FLAGS})
target_include_directories(roast PRIVATE src/)

# set_target_properties(roast PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
