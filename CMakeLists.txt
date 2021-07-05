cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_COMPILER g++)

set(SOURCES
    src/NeoMIPS.cpp
    src/argumentprocessor.cpp
    src/constraints.cpp
    src/error.cpp
    src/executioncontext.cpp
    src/filereader.cpp
    src/lexer_util.cpp
    src/lexer.cpp
    src/option.cpp
    src/Preprocessor.cpp
    src/StringUtil.cpp
    src/token.cpp
    src/util.cpp
)

project(NeoMIPS CXX)

include_directories(.)

if (MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_executable(neomips ${SOURCES})
