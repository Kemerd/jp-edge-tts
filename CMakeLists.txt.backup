cmake_minimum_required(VERSION 3.18)
project(jp_edge_tts VERSION 1.0.0 LANGUAGES CXX C)

# ==========================================
# Project Settings and Options
# ==========================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Build options
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_EXAMPLES "Build example applications" ON)
option(USE_GPU "Enable GPU support (requires CUDA)" OFF)
option(USE_MECAB "Enable MeCab for Japanese morphological analysis" ON)
option(ENABLE_PROFILING "Enable performance profiling" OFF)

# ==========================================
# Platform-Specific Configuration
# ==========================================
if(WIN32)
    # Windows-specific settings
    add_definitions(-D_WIN32_WINNT=0x0601)  # Windows 7 minimum
    add_definitions(-DNOMINMAX)  # Prevent Windows.h from defining min/max macros
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)  # Disable MSVC secure warnings

    # Set Windows runtime library
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    # Enable parallel compilation on MSVC
    if(MSVC)
        add_compile_options(/MP)
        add_compile_options(/utf-8)  # Set source and execution character sets to UTF-8
    endif()
elseif(APPLE)
    # macOS-specific settings
    set(CMAKE_MACOSX_RPATH ON)
    set(CMAKE_SKIP_BUILD_RPATH FALSE)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

    # Universal binary support for Apple Silicon and Intel
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
    endif()
elseif(UNIX AND NOT APPLE)
    # Linux-specific settings
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
    find_package(Threads REQUIRED)
endif()

# ==========================================
# Compiler Flags and Optimizations
# ==========================================
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Common flags for GCC and Clang
    add_compile_options(-Wall -Wextra -Wpedantic)
    add_compile_options(-Wno-unused-parameter -Wno-unused-variable)

    # Optimization flags for Release builds
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -march=native -mtune=native)
        add_compile_options(-ffast-math -funroll-loops)

        # Link-time optimization
        include(CheckIPOSupported)
        check_ipo_supported(RESULT ipo_supported OUTPUT error)
        if(ipo_supported)
            set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
        endif()
    endif()

    # Debug flags
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0)
        add_compile_options(-fsanitize=address -fsanitize=undefined)
        add_link_options(-fsanitize=address -fsanitize=undefined)
    endif()
elseif(MSVC)
    # MSVC-specific flags
    add_compile_options(/W4)

    # Optimization flags for Release builds
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(/O2 /Ob2 /Oi /Ot /Oy /GL)
        add_link_options(/LTCG)
    endif()

    # Debug flags
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(/Od /RTC1)
    endif()
endif()

# ==========================================
# Find and Configure Dependencies
# ==========================================

# ONNX Runtime configuration
set(ONNXRUNTIME_ROOT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/.github/onnxruntime")

# Build ONNX Runtime from source if submodule exists
if(EXISTS "${ONNXRUNTIME_ROOT_PATH}/cmake/CMakeLists.txt")
    message(STATUS "Building ONNX Runtime from source...")

    # ONNX Runtime build options
    set(onnxruntime_BUILD_SHARED_LIB OFF CACHE BOOL "" FORCE)
    set(onnxruntime_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
    set(onnxruntime_BUILD_CSHARP OFF CACHE BOOL "" FORCE)
    set(onnxruntime_BUILD_JAVA OFF CACHE BOOL "" FORCE)
    set(onnxruntime_BUILD_NODEJS OFF CACHE BOOL "" FORCE)
    set(onnxruntime_BUILD_OBJC OFF CACHE BOOL "" FORCE)
    set(onnxruntime_BUILD_PYTHON OFF CACHE BOOL "" FORCE)
    set(onnxruntime_USE_CUDA ${USE_GPU} CACHE BOOL "" FORCE)
    set(onnxruntime_USE_FULL_PROTOBUF OFF CACHE BOOL "" FORCE)
    set(onnxruntime_MINIMAL_BUILD OFF CACHE BOOL "" FORCE)
    set(onnxruntime_DISABLE_CONTRIB_OPS OFF CACHE BOOL "" FORCE)
    set(onnxruntime_DISABLE_ML_OPS OFF CACHE BOOL "" FORCE)

    # Add ONNX Runtime as subdirectory
    add_subdirectory(${ONNXRUNTIME_ROOT_PATH}/cmake onnxruntime EXCLUDE_FROM_ALL)

    # Set include directories
    set(ONNXRUNTIME_INCLUDE_DIRS
        "${ONNXRUNTIME_ROOT_PATH}/include/onnxruntime/core/session"
        "${ONNXRUNTIME_ROOT_PATH}/include/onnxruntime"
        "${ONNXRUNTIME_ROOT_PATH}/include"
    )

    # Link libraries
    if(WIN32)
        set(ONNXRUNTIME_LIBRARIES onnxruntime)
    else()
        set(ONNXRUNTIME_LIBRARIES onnxruntime)
    endif()
else()
    # Try to find prebuilt ONNX Runtime
    message(STATUS "Looking for prebuilt ONNX Runtime...")
    find_path(ONNXRUNTIME_INCLUDE_DIRS onnxruntime_cxx_api.h
        PATHS
            ${ONNXRUNTIME_ROOT_PATH}/include/onnxruntime/core/session
            /usr/local/include/onnxruntime
            /usr/include/onnxruntime
    )

    find_library(ONNXRUNTIME_LIBRARIES
        NAMES onnxruntime
        PATHS
            ${ONNXRUNTIME_ROOT_PATH}/lib
            /usr/local/lib
            /usr/lib
    )

    if(NOT ONNXRUNTIME_INCLUDE_DIRS OR NOT ONNXRUNTIME_LIBRARIES)
        message(FATAL_ERROR "ONNX Runtime not found. Please initialize the git submodule or install ONNX Runtime.")
    endif()
endif()

message(STATUS "ONNX Runtime include: ${ONNXRUNTIME_INCLUDE_DIRS}")
message(STATUS "ONNX Runtime libraries: ${ONNXRUNTIME_LIBRARIES}")

# Find nlohmann/json for JSON parsing
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not found, using bundled version")
    # We'll include a header-only version in third_party
    set(JSON_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/json/include")
else()
    message(STATUS "Found nlohmann_json")
endif()

# MeCab for Japanese text processing - compile from source
if(USE_MECAB)
    message(STATUS "Building MeCab from source...")
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/MeCab.cmake)

    if(MECAB_LIBRARY)
        message(STATUS "MeCab will be built from source")
        add_definitions(-DUSE_MECAB)
    else()
        message(WARNING "MeCab source not found, Japanese text segmentation will use fallback method")
        set(USE_MECAB OFF)
    endif()
endif()

# ==========================================
# Project Structure
# ==========================================

# Create output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${ONNXRUNTIME_INCLUDE_DIRS}
)

if(JSON_INCLUDE_DIR)
    include_directories(${JSON_INCLUDE_DIR})
endif()

if(USE_MECAB AND MECAB_INCLUDE_DIR)
    include_directories(${MECAB_INCLUDE_DIR})
endif()

# ==========================================
# Core Library
# ==========================================

# Source files - matching header organization
set(CORE_SOURCES
    # Core module
    src/core/tts_engine.cpp
    src/core/session_manager.cpp
    src/core/voice_manager.cpp
    src/core/cache_manager.cpp

    # Phonemizer module
    src/phonemizer/japanese_phonemizer.cpp
    src/phonemizer/dictionary_lookup.cpp
    src/phonemizer/phonemizer_onnx.cpp

    # Tokenizer module
    src/tokenizer/ipa_tokenizer.cpp
    src/tokenizer/vocab_manager.cpp
    src/tokenizer/mecab_wrapper.cpp

    # Audio module
    src/audio/audio_processor.cpp
    src/audio/wav_writer.cpp

    # Utilities
    src/utils/string_utils.cpp
    src/utils/file_utils.cpp
    src/utils/thread_pool.cpp

    # C API wrapper
    src/c_api/jp_edge_tts_c_api.cpp
)

# Header files - organized by module
set(CORE_HEADERS
    # Core module
    include/jp_edge_tts/core/tts_engine.h
    include/jp_edge_tts/core/session_manager.h
    include/jp_edge_tts/core/voice_manager.h
    include/jp_edge_tts/core/cache_manager.h

    # Phonemizer module
    include/jp_edge_tts/phonemizer/japanese_phonemizer.h
    include/jp_edge_tts/phonemizer/dictionary_lookup.h
    include/jp_edge_tts/phonemizer/phonemizer_onnx.h

    # Tokenizer module
    include/jp_edge_tts/tokenizer/ipa_tokenizer.h
    include/jp_edge_tts/tokenizer/vocab_manager.h
    include/jp_edge_tts/tokenizer/mecab_wrapper.h

    # Audio module
    include/jp_edge_tts/audio/audio_processor.h
    include/jp_edge_tts/audio/wav_writer.h

    # Utilities
    include/jp_edge_tts/utils/string_utils.h
    include/jp_edge_tts/utils/file_utils.h
    include/jp_edge_tts/utils/thread_pool.h

    # Common headers
    include/jp_edge_tts/types.h
    include/jp_edge_tts/config.h
    include/jp_edge_tts/jp_edge_tts_c_api.h
)

# Create the core library
add_library(jp_edge_tts_core ${CORE_SOURCES} ${CORE_HEADERS})

# Link dependencies
target_link_libraries(jp_edge_tts_core
    PUBLIC
        ${ONNXRUNTIME_LIBRARIES}
)

if(USE_MECAB AND MECAB_LIBRARY)
    target_link_libraries(jp_edge_tts_core PUBLIC ${MECAB_LIBRARY})
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(jp_edge_tts_core PUBLIC Threads::Threads)
endif()

# Set properties
set_target_properties(jp_edge_tts_core PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER "${CORE_HEADERS}"
)

# ==========================================
# Example Applications
# ==========================================

if(BUILD_EXAMPLES)
    # CLI TTS application
    add_executable(jp_tts_cli examples/cli/main.cpp)
    target_link_libraries(jp_tts_cli PRIVATE jp_edge_tts_core)

    # Benchmark utility
    add_executable(jp_tts_benchmark examples/benchmark/benchmark.cpp)
    target_link_libraries(jp_tts_benchmark PRIVATE jp_edge_tts_core)

    # Simple API example
    add_executable(jp_tts_simple examples/simple/simple_tts.cpp)
    target_link_libraries(jp_tts_simple PRIVATE jp_edge_tts_core)
endif()

# ==========================================
# Unit Tests
# ==========================================

if(BUILD_TESTS)
    enable_testing()

    # Find or fetch Google Test
    find_package(GTest QUIET)
    if(NOT GTest_FOUND)
        include(FetchContent)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG release-1.12.1
        )
        FetchContent_MakeAvailable(googletest)
    endif()

    # Test executables
    add_executable(test_phonemizer tests/test_phonemizer.cpp)
    target_link_libraries(test_phonemizer jp_edge_tts_core GTest::gtest_main)

    add_executable(test_tokenizer tests/test_tokenizer.cpp)
    target_link_libraries(test_tokenizer jp_edge_tts_core GTest::gtest_main)

    add_executable(test_audio tests/test_audio.cpp)
    target_link_libraries(test_audio jp_edge_tts_core GTest::gtest_main)

    # Add tests
    add_test(NAME PhonemizerTest COMMAND test_phonemizer)
    add_test(NAME TokenizerTest COMMAND test_tokenizer)
    add_test(NAME AudioTest COMMAND test_audio)
endif()

# ==========================================
# Installation
# ==========================================

include(GNUInstallDirs)

# Install the library
install(TARGETS jp_edge_tts_core
    EXPORT jp_edge_tts_targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/jp_edge_tts
)

# Install example applications
if(BUILD_EXAMPLES)
    install(TARGETS jp_tts_cli jp_tts_benchmark jp_tts_simple
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

# Install data files
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data
    DESTINATION ${CMAKE_INSTALL_DATADIR}/jp_edge_tts
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/models
    DESTINATION ${CMAKE_INSTALL_DATADIR}/jp_edge_tts
)

# Export targets
install(EXPORT jp_edge_tts_targets
    FILE jp_edge_tts_targets.cmake
    NAMESPACE jp_edge_tts::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/jp_edge_tts
)

# Create and install package config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/jp_edge_tts_config_version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_file(cmake/jp_edge_tts_config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/jp_edge_tts_config.cmake"
    @ONLY
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/jp_edge_tts_config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/jp_edge_tts_config_version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/jp_edge_tts
)

# ==========================================
# Summary
# ==========================================

message(STATUS "========================================")
message(STATUS "JP Edge TTS Configuration Summary:")
message(STATUS "  Version:           ${PROJECT_VERSION}")
message(STATUS "  Build Type:        ${CMAKE_BUILD_TYPE}")
message(STATUS "  Install Prefix:    ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Compiler:          ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  C++ Standard:      ${CMAKE_CXX_STANDARD}")
message(STATUS "Options:")
message(STATUS "  Build Tests:       ${BUILD_TESTS}")
message(STATUS "  Build Examples:    ${BUILD_EXAMPLES}")
message(STATUS "  Use GPU:           ${USE_GPU}")
message(STATUS "  Use MeCab:         ${USE_MECAB}")
message(STATUS "  Enable Profiling:  ${ENABLE_PROFILING}")
message(STATUS "========================================")