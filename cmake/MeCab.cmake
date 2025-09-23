# ==========================================
# MeCab Integration for JP Edge TTS
# ==========================================

# MeCab source directory
set(MECAB_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.github/mecab/mecab/src")

if(NOT EXISTS "${MECAB_SOURCE_DIR}/mecab.h")
    message(FATAL_ERROR "MeCab source not found. Please run: git submodule update --init --recursive")
endif()

# MeCab source files (core functionality only, no dictionary tools)
set(MECAB_SOURCES
    ${MECAB_SOURCE_DIR}/viterbi.cpp
    ${MECAB_SOURCE_DIR}/tagger.cpp
    ${MECAB_SOURCE_DIR}/utils.cpp
    ${MECAB_SOURCE_DIR}/feature_index.cpp
    ${MECAB_SOURCE_DIR}/dictionary.cpp
    ${MECAB_SOURCE_DIR}/connector.cpp
    ${MECAB_SOURCE_DIR}/context_id.cpp
    ${MECAB_SOURCE_DIR}/char_property.cpp
    ${MECAB_SOURCE_DIR}/tokenizer.cpp
    ${MECAB_SOURCE_DIR}/nbest_generator.cpp
    ${MECAB_SOURCE_DIR}/dictionary_rewriter.cpp
    ${MECAB_SOURCE_DIR}/writer.cpp
    ${MECAB_SOURCE_DIR}/string_buffer.cpp
    ${MECAB_SOURCE_DIR}/param.cpp
    ${MECAB_SOURCE_DIR}/libmecab.cpp
)

# Create MeCab static library
add_library(mecab_static STATIC ${MECAB_SOURCES})

# Configure MeCab
target_compile_definitions(mecab_static PRIVATE
    DLL_EXPORT
    HAVE_GETENV
    HAVE_WINDOWS_H=$<BOOL:${WIN32}>
    MECAB_DEFAULT_RC="mecabrc"
)

# Platform-specific settings
if(WIN32)
    target_compile_definitions(mecab_static PRIVATE
        _CRT_SECURE_NO_DEPRECATE
        _CRT_NONSTDC_NO_DEPRECATE
        NOMINMAX
    )
    # Disable specific warnings for MeCab on MSVC
    if(MSVC)
        target_compile_options(mecab_static PRIVATE
            /wd4018  # signed/unsigned mismatch
            /wd4267  # conversion from size_t
            /wd4996  # deprecated functions
            /wd4244  # conversion warnings
        )
    endif()
elseif(UNIX)
    target_compile_definitions(mecab_static PRIVATE
        HAVE_UNISTD_H
        HAVE_SYS_TYPES_H
        HAVE_SYS_STAT_H
        HAVE_FCNTL_H
        HAVE_STRING_H
        HAVE_SYS_MMAN_H
        HAVE_DIRENT_H
        HAVE_STDINT_H
    )
endif()

# Set C++ standard for MeCab
set_target_properties(mecab_static PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

# Include directories
target_include_directories(mecab_static PUBLIC
    ${MECAB_SOURCE_DIR}
)

# Export MeCab for use in main project
set(MECAB_INCLUDE_DIR ${MECAB_SOURCE_DIR} PARENT_SCOPE)
set(MECAB_LIBRARY mecab_static PARENT_SCOPE)

# Install MeCab headers
install(FILES ${MECAB_SOURCE_DIR}/mecab.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/jp_edge_tts/third_party
)