cmake_minimum_required(VERSION 3.10)

# Project name
project(MyProject)

# Add the executable and specify source files from the /src directory
add_executable(main
    src/main.c
    src/built_in_functions.c
    src/helpers.c
    src/fileFunctions.c
)

# Include header files from the /include directory
target_include_directories(main PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(main PRIVATE -Wall -Werror)
endif()
