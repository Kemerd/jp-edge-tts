/**
 * @file config.h
 * @brief Configuration structures and constants for JP Edge TTS
 * D Everett Hinton
 * @date 2025
 *
 * @copyright MIT License
 */

#ifndef JP_EDGE_TTS_CONFIG_H
#define JP_EDGE_TTS_CONFIG_H

#include <string>

namespace jp_edge_tts {

// ==========================================
// Version Information
// ==========================================

#define JP_EDGE_TTS_VERSION_MAJOR 1
#define JP_EDGE_TTS_VERSION_MINOR 0
#define JP_EDGE_TTS_VERSION_PATCH 0
#define JP_EDGE_TTS_VERSION_STRING "1.0.0"

// ==========================================
// Default Paths
// ==========================================

constexpr const char* DEFAULT_KOKORO_MODEL = "models/kokoro-v1.0.int8.onnx";
constexpr const char* DEFAULT_PHONEMIZER_MODEL = "models/phonemizer.onnx";
constexpr const char* DEFAULT_DICTIONARY = "data/ja_phonemes.json";
constexpr const char* DEFAULT_TOKENIZER_VOCAB = "models/tokenizer_vocab.json";
constexpr const char* DEFAULT_VOICES_DIR = "models/voices";
constexpr const char* DEFAULT_OUTPUT_DIR = "output";

// ==========================================
// Audio Settings
// ==========================================

constexpr int DEFAULT_SAMPLE_RATE = 24000;
constexpr int DEFAULT_CHANNELS = 1;  // Mono
constexpr int DEFAULT_BITS_PER_SAMPLE = 16;
constexpr float DEFAULT_SPEED = 1.0f;
constexpr float DEFAULT_PITCH = 1.0f;
constexpr float DEFAULT_VOLUME = 1.0f;

// ==========================================
// Performance Settings
// ==========================================

constexpr int DEFAULT_MAX_CONCURRENT_REQUESTS = 4;
constexpr int DEFAULT_QUEUE_SIZE = 100;
constexpr size_t DEFAULT_CACHE_SIZE_MB = 100;
constexpr int DEFAULT_CACHE_TTL_SECONDS = 3600;
constexpr size_t DEFAULT_MAX_TEXT_LENGTH = 1000;

// ==========================================
// Model Settings
// ==========================================

constexpr int KOKORO_STYLE_DIM = 128;  // Style vector dimension
constexpr int MAX_TOKEN_LENGTH = 500;   // Maximum token sequence length
constexpr int PHONEME_VOCAB_SIZE = 200; // Approximate phoneme vocabulary

// ==========================================
// Platform-Specific Settings
// ==========================================

#ifdef _WIN32
    #define JP_PATH_SEPARATOR "\\"
#else
    #define JP_PATH_SEPARATOR "/"
#endif

} // namespace jp_edge_tts

#endif // JP_EDGE_TTS_CONFIG_H