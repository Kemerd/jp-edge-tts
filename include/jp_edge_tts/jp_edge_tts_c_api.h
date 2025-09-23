/**
 * @file jp_edge_tts_c_api.h
 * @brief C API wrapper for JP Edge TTS - FFI compatible interface
 * D Everett Hinton
 * @date 2025
 *
 * @details This header provides a pure C interface for the JP Edge TTS library,
 * suitable for FFI (Foreign Function Interface) usage from other languages
 * like Dart, Python, Rust, Go, C#, Java, etc. All functions use C-compatible types
 * and follow standard C calling conventions.
 *
 * @copyright MIT License
 */

#ifndef JP_EDGE_TTS_C_API_H
#define JP_EDGE_TTS_C_API_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================
 * Export/Import Macros for DLL
 * ==========================================*/

#ifdef _WIN32
    #ifdef JP_EDGE_TTS_EXPORTS
        #define JP_EDGE_TTS_API __declspec(dllexport)
    #else
        #define JP_EDGE_TTS_API __declspec(dllimport)
    #endif
#else
    #define JP_EDGE_TTS_API __attribute__((visibility("default")))
#endif

/* Calling convention for Windows compatibility */
#ifdef _WIN32
    #define JP_EDGE_TTS_CALL __cdecl
#else
    #define JP_EDGE_TTS_CALL
#endif

/* ==========================================
 * Type Definitions
 * ==========================================*/

/**
 * @brief Opaque handle to TTS engine instance
 */
typedef void* jp_tts_engine_t;

/**
 * @brief Opaque handle to TTS result
 */
typedef void* jp_tts_result_t;

/**
 * @brief Status codes returned by API functions
 */
typedef enum {
    JP_TTS_OK = 0,                    ///< Success
    JP_TTS_ERROR_INVALID_INPUT = 1,   ///< Invalid input provided
    JP_TTS_ERROR_MODEL_NOT_LOADED = 2, ///< Model not loaded
    JP_TTS_ERROR_INFERENCE_FAILED = 3, ///< Inference failed
    JP_TTS_ERROR_MEMORY = 4,          ///< Memory allocation failed
    JP_TTS_ERROR_FILE_NOT_FOUND = 5,  ///< File not found
    JP_TTS_ERROR_UNSUPPORTED = 6,     ///< Unsupported operation
    JP_TTS_ERROR_NOT_INITIALIZED = 7,  ///< Engine not initialized
    JP_TTS_ERROR_TIMEOUT = 8,         ///< Operation timed out
    JP_TTS_ERROR_UNKNOWN = -1         ///< Unknown error
} jp_tts_status_t;

/**
 * @brief Audio format types
 */
typedef enum {
    JP_TTS_FORMAT_WAV_PCM16 = 0,      ///< 16-bit PCM WAV
    JP_TTS_FORMAT_WAV_FLOAT32 = 1,    ///< 32-bit float WAV
    JP_TTS_FORMAT_RAW_PCM16 = 2,      ///< Raw 16-bit PCM
    JP_TTS_FORMAT_RAW_FLOAT32 = 3     ///< Raw 32-bit float
} jp_tts_audio_format_t;

/**
 * @brief TTS configuration structure
 */
typedef struct {
    const char* kokoro_model_path;    ///< Path to Kokoro ONNX model
    const char* phonemizer_model_path; ///< Path to phonemizer ONNX model
    const char* dictionary_path;       ///< Path to phoneme dictionary
    const char* tokenizer_vocab_path;  ///< Path to tokenizer vocabulary
    const char* voices_dir;            ///< Directory containing voice files

    int32_t max_concurrent_requests;   ///< Max parallel synthesis (default: 4)
    int32_t onnx_inter_threads;        ///< ONNX inter-op threads (0 = auto)
    int32_t onnx_intra_threads;        ///< ONNX intra-op threads (0 = auto)
    bool enable_gpu;                   ///< Enable GPU acceleration

    bool enable_cache;                 ///< Enable result caching
    size_t max_cache_size_mb;          ///< Max cache size in MB
    int32_t cache_ttl_seconds;         ///< Cache time-to-live

    int32_t target_sample_rate;        ///< Output sample rate (default: 24000)
    bool normalize_audio;              ///< Normalize output audio
    float silence_threshold;           ///< Silence detection threshold

    bool enable_mecab;                 ///< Use MeCab for segmentation
    bool normalize_numbers;            ///< Convert numbers to words
    bool verbose;                      ///< Enable verbose logging
} jp_tts_config_t;

/**
 * @brief TTS synthesis request parameters
 */
typedef struct {
    const char* text;                  ///< Input text (UTF-8 Japanese)
    const char* voice_id;              ///< Voice ID to use
    float speed;                       ///< Speaking speed (0.5-2.0)
    float pitch;                       ///< Pitch adjustment (0.5-2.0)
    float volume;                      ///< Volume (0.0-1.0)
    jp_tts_audio_format_t format;     ///< Output audio format

    const char* ipa_phonemes;          ///< Optional: pre-computed IPA phonemes
    int32_t vocabulary_id;             ///< Optional: vocabulary ID (-1 = none)
    bool use_cache;                    ///< Use caching
} jp_tts_request_t;

/**
 * @brief Audio data information
 */
typedef struct {
    float* samples;                    ///< Audio samples (normalized -1 to 1)
    size_t sample_count;               ///< Number of samples
    int32_t sample_rate;               ///< Sample rate in Hz
    int32_t channels;                  ///< Number of channels
    int32_t duration_ms;               ///< Duration in milliseconds
} jp_tts_audio_data_t;

/**
 * @brief Voice information
 */
typedef struct {
    char id[64];                       ///< Voice identifier
    char name[128];                    ///< Display name
    char language[8];                  ///< Language code
    char gender[16];                   ///< Voice gender
    float default_speed;               ///< Default speed
    float default_pitch;               ///< Default pitch
} jp_tts_voice_info_t;

/* ==========================================
 * Engine Lifecycle Functions
 * ==========================================*/

/**
 * @brief Create a new TTS engine instance
 *
 * @param config Configuration settings (can be NULL for defaults)
 * @return Engine handle, or NULL on failure
 */
JP_EDGE_TTS_API jp_tts_engine_t JP_EDGE_TTS_CALL
jp_tts_create_engine(const jp_tts_config_t* config);

/**
 * @brief Initialize the TTS engine
 *
 * @param engine Engine handle
 * @return Status code
 */
JP_EDGE_TTS_API jp_tts_status_t JP_EDGE_TTS_CALL
jp_tts_initialize(jp_tts_engine_t engine);

/**
 * @brief Check if engine is initialized
 *
 * @param engine Engine handle
 * @return true if initialized, false otherwise
 */
JP_EDGE_TTS_API bool JP_EDGE_TTS_CALL
jp_tts_is_initialized(jp_tts_engine_t engine);

/**
 * @brief Destroy TTS engine and free resources
 *
 * @param engine Engine handle
 */
JP_EDGE_TTS_API void JP_EDGE_TTS_CALL
jp_tts_destroy_engine(jp_tts_engine_t engine);

/* ==========================================
 * Voice Management
 * ==========================================*/

/**
 * @brief Load a voice from file
 *
 * @param engine Engine handle
 * @param voice_path Path to voice file
 * @return Status code
 */
JP_EDGE_TTS_API jp_tts_status_t JP_EDGE_TTS_CALL
jp_tts_load_voice(jp_tts_engine_t engine, const char* voice_path);

/**
 * @brief Get number of available voices
 *
 * @param engine Engine handle
 * @return Number of loaded voices
 */
JP_EDGE_TTS_API int32_t JP_EDGE_TTS_CALL
jp_tts_get_voice_count(jp_tts_engine_t engine);

/**
 * @brief Get voice information by index
 *
 * @param engine Engine handle
 * @param index Voice index (0-based)
 * @param info Output voice information
 * @return Status code
 */
JP_EDGE_TTS_API jp_tts_status_t JP_EDGE_TTS_CALL
jp_tts_get_voice_info(jp_tts_engine_t engine, int32_t index,
                      jp_tts_voice_info_t* info);

/**
 * @brief Set default voice by ID
 *
 * @param engine Engine handle
 * @param voice_id Voice identifier
 * @return Status code
 */
JP_EDGE_TTS_API jp_tts_status_t JP_EDGE_TTS_CALL
jp_tts_set_default_voice(jp_tts_engine_t engine, const char* voice_id);

/* ==========================================
 * TTS Synthesis
 * ==========================================*/

/**
 * @brief Simple text-to-speech synthesis
 *
 * @param engine Engine handle
 * @param text Japanese text to synthesize (UTF-8)
 * @param voice_id Voice ID (NULL for default)
 * @return Result handle, or NULL on failure
 */
JP_EDGE_TTS_API jp_tts_result_t JP_EDGE_TTS_CALL
jp_tts_synthesize_simple(jp_tts_engine_t engine, const char* text,
                         const char* voice_id);

/**
 * @brief Advanced synthesis with full control
 *
 * @param engine Engine handle
 * @param request Request parameters
 * @return Result handle, or NULL on failure
 */
JP_EDGE_TTS_API jp_tts_result_t JP_EDGE_TTS_CALL
jp_tts_synthesize(jp_tts_engine_t engine, const jp_tts_request_t* request);

/**
 * @brief Synthesize from JSON request
 *
 * @param engine Engine handle
 * @param json_request JSON string with request parameters
 * @return Result handle, or NULL on failure
 *
 * @details JSON format:
 * {
 *   "text": "Japanese text",
 *   "voice_id": "jf_alpha",
 *   "speed": 1.0,
 *   "pitch": 1.0,
 *   "volume": 1.0,
 *   "phonemes": "optional IPA phonemes"
 * }
 */
JP_EDGE_TTS_API jp_tts_result_t JP_EDGE_TTS_CALL
jp_tts_synthesize_json(jp_tts_engine_t engine, const char* json_request);

/* ==========================================
 * Result Handling
 * ==========================================*/

/**
 * @brief Get status of synthesis result
 *
 * @param result Result handle
 * @return Status code
 */
JP_EDGE_TTS_API jp_tts_status_t JP_EDGE_TTS_CALL
jp_tts_result_get_status(jp_tts_result_t result);

/**
 * @brief Get audio data from result
 *
 * @param result Result handle
 * @param audio_data Output audio data structure
 * @return Status code
 */
JP_EDGE_TTS_API jp_tts_status_t JP_EDGE_TTS_CALL
jp_tts_result_get_audio(jp_tts_result_t result,
                        jp_tts_audio_data_t* audio_data);

/**
 * @brief Save result audio to file
 *
 * @param result Result handle
 * @param filepath Output file path
 * @param format Audio format
 * @return Status code
 */
JP_EDGE_TTS_API jp_tts_status_t JP_EDGE_TTS_CALL
jp_tts_result_save_to_file(jp_tts_result_t result, const char* filepath,
                           jp_tts_audio_format_t format);

/**
 * @brief Get WAV data as byte array
 *
 * @param result Result handle
 * @param buffer Output buffer (NULL to query size)
 * @param buffer_size In: buffer size, Out: data size
 * @return Status code
 */
JP_EDGE_TTS_API jp_tts_status_t JP_EDGE_TTS_CALL
jp_tts_result_get_wav_bytes(jp_tts_result_t result, uint8_t* buffer,
                            size_t* buffer_size);

/**
 * @brief Get phonemes from result
 *
 * @param result Result handle
 * @param buffer Output buffer for phonemes (NULL to query size)
 * @param buffer_size In: buffer size, Out: string length
 * @return Status code
 */
JP_EDGE_TTS_API jp_tts_status_t JP_EDGE_TTS_CALL
jp_tts_result_get_phonemes(jp_tts_result_t result, char* buffer,
                           size_t* buffer_size);

/**
 * @brief Free synthesis result
 *
 * @param result Result handle
 */
JP_EDGE_TTS_API void JP_EDGE_TTS_CALL
jp_tts_result_free(jp_tts_result_t result);

/* ==========================================
 * Text Processing
 * ==========================================*/

/**
 * @brief Convert Japanese text to IPA phonemes
 *
 * @param engine Engine handle
 * @param text Input text
 * @param phonemes Output buffer (NULL to query size)
 * @param buffer_size In: buffer size, Out: string length
 * @return Status code
 */
JP_EDGE_TTS_API jp_tts_status_t JP_EDGE_TTS_CALL
jp_tts_text_to_phonemes(jp_tts_engine_t engine, const char* text,
                        char* phonemes, size_t* buffer_size);

/**
 * @brief Normalize Japanese text
 *
 * @param engine Engine handle
 * @param text Input text
 * @param normalized Output buffer (NULL to query size)
 * @param buffer_size In: buffer size, Out: string length
 * @return Status code
 */
JP_EDGE_TTS_API jp_tts_status_t JP_EDGE_TTS_CALL
jp_tts_normalize_text(jp_tts_engine_t engine, const char* text,
                      char* normalized, size_t* buffer_size);

/* ==========================================
 * Cache Management
 * ==========================================*/

/**
 * @brief Clear all cached results
 *
 * @param engine Engine handle
 */
JP_EDGE_TTS_API void JP_EDGE_TTS_CALL
jp_tts_clear_cache(jp_tts_engine_t engine);

/**
 * @brief Get cache statistics
 *
 * @param engine Engine handle
 * @param total_entries Output: total cache entries
 * @param total_size_mb Output: total size in MB
 * @param hit_rate Output: cache hit rate (0-1)
 * @return Status code
 */
JP_EDGE_TTS_API jp_tts_status_t JP_EDGE_TTS_CALL
jp_tts_get_cache_stats(jp_tts_engine_t engine, size_t* total_entries,
                       size_t* total_size_mb, float* hit_rate);

/* ==========================================
 * Utility Functions
 * ==========================================*/

/**
 * @brief Get library version string
 *
 * @return Version string (do not free)
 */
JP_EDGE_TTS_API const char* JP_EDGE_TTS_CALL
jp_tts_get_version(void);

/**
 * @brief Check if GPU is available
 *
 * @return true if GPU is available
 */
JP_EDGE_TTS_API bool JP_EDGE_TTS_CALL
jp_tts_is_gpu_available(void);

/**
 * @brief Get last error message
 *
 * @param engine Engine handle (can be NULL)
 * @return Error message string (do not free)
 */
JP_EDGE_TTS_API const char* JP_EDGE_TTS_CALL
jp_tts_get_last_error(jp_tts_engine_t engine);

/**
 * @brief Set log level
 *
 * @param level Log level (0=off, 1=error, 2=warn, 3=info, 4=debug)
 */
JP_EDGE_TTS_API void JP_EDGE_TTS_CALL
jp_tts_set_log_level(int32_t level);

/**
 * @brief Set log callback function
 *
 * @param callback Function to receive log messages
 * @param user_data User data passed to callback
 */
typedef void (*jp_tts_log_callback_t)(int32_t level, const char* message,
                                      void* user_data);
JP_EDGE_TTS_API void JP_EDGE_TTS_CALL
jp_tts_set_log_callback(jp_tts_log_callback_t callback, void* user_data);

#ifdef __cplusplus
}
#endif

#endif /* JP_EDGE_TTS_C_API_H */