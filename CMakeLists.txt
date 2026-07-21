cmake_minimum_required(VERSION 3.15)
project(JosiahChessEngine VERSION 10.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Default to Release build if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Performance optimization flags
if(MSVC)
    # Microsoft Visual C++ optimizations
    add_compile_options(/O2 /Oi /Ot /GL)
    add_link_options(/LTCG)
else()
    # GCC/Clang optimizations
    add_compile_options(-O3 -march=native -flto)
    
    # Platform-specific optimizations
    if(APPLE)
        add_compile_options(-fno-exceptions)
    endif()
endif()

# Define version
set(ENGINE_VERSION "10.0")
add_compile_definitions(ENGINE_VERSION="${ENGINE_VERSION}")

# Core executable target
add_executable(josiah_engine
    src/main.c
    src/position.c
    src/search.c
    src/evaluate.c
    src/uci.c
    src/bitboard.c
    src/magic.c
    src/movegen.c
    src/zobrist.c
    nnue/inference.c
    syzygy/tbprobe.c
)

# Include directories
target_include_directories(josiah_engine 
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/nnue
        ${CMAKE_CURRENT_SOURCE_DIR}/syzygy
)

# Platform-specific libraries
if(UNIX AND NOT APPLE)
    # Link math library on Linux
    target_link_libraries(josiah_engine PRIVATE m)
elseif(APPLE)
    # macOS-specific frameworks if needed
    target_link_libraries(josiah_engine PRIVATE)
endif()

# Set output directory
set_target_properties(josiah_engine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Copy NNUE model to build directory
add_custom_command(TARGET josiah_engine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/nnue"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/nnue/768_quant_9_18_50_1024.nnue"
        "${CMAKE_BINARY_DIR}/bin/nnue/768_quant_9_18_50_1024.nnue"
    COMMENT "Copying NNUE model to build directory..."
)

# Copy opening book to build directory
add_custom_command(TARGET josiah_engine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/book.txt"
        "${CMAKE_BINARY_DIR}/bin/book.txt"
    COMMENT "Copying opening book to build directory..."
)

# Installation targets
install(TARGETS josiah_engine
    RUNTIME DESTINATION bin
)

install(DIRECTORY nnue/
    DESTINATION share/josiah_engine/nnue
    FILES_MATCHING PATTERN "*.nnue"
)

# Build information
message(STATUS "========================================")
message(STATUS "Josiah Chess Engine v${ENGINE_VERSION}")
message(STATUS "========================================")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C Standard: ${CMAKE_C_STANDARD}")
message(STATUS "Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Syzygy Path: setoption name SyzygyPath value <path-to-tables>")
message(STATUS "========================================")
