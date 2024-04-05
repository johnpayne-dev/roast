cmake_minimum_required(VERSION 3.21)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

project(roast)

set(PROJECT_FILES
    README.md
    src/main.c
    src/assembly/llir.c
    src/assembly/llir.h
    src/parser/ast.c
    src/parser/ast.h
    src/parser/parser.c
    src/parser/parser.h
    src/scanner/scanner.c
    src/scanner/scanner.h
    src/scanner/token.c
    src/scanner/token.h
    src/semantics/ir.c
    src/semantics/ir.h
    src/semantics/semantics.c
    src/semantics/semantics.h
    src/semantics/symbol_table.c
    src/semantics/symbol_table.h)

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
