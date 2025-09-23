#ifndef JP_EDGE_TTS_TYPES_H
#define JP_EDGE_TTS_TYPES_H

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <chrono>
#include <cstdint>

namespace jp_edge_tts {

// ==========================================
// Enumerations
// ==========================================

// TTS synthesis status codes
enum class Status {
    OK = 0,
    ERROR_INVALID_INPUT,
    ERROR_MODEL_NOT_LOADED,
    ERROR_INFERENCE_FAILED,
    ERROR_MEMORY_ALLOCATION,
    ERROR_FILE_NOT_FOUND,
    ERROR_UNSUPPORTED_FORMAT,
    ERROR_CACHE_MISS,
    ERROR_TIMEOUT,
    ERROR_NOT_INITIALIZED,
    ERROR_UNKNOWN
};

// Voice characteristics
enum class VoiceGender {
    MALE,
    FEMALE,
    NEUTRAL
};

// Audio output formats
enum class AudioFormat {
    WAV_PCM16,    // 16-bit PCM WAV
    WAV_FLOAT32,  // 32-bit float WAV
    RAW_PCM16,    // Raw 16-bit PCM
    RAW_FLOAT32   // Raw 32-bit float
};

// Processing priority levels
enum class Priority {
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    CRITICAL = 3
};

// ==========================================
// Core Data Structures
// ==========================================

// Voice configuration
struct Voice {
    std::string id;                              // Unique voice identifier
    std::string name;                            // Display name
    VoiceGender gender = VoiceGender::NEUTRAL;   // Voice gender
    std::string language = "ja";                 // Language code (default Japanese)
    std::vector<float> style_vector;             // Style embedding vector (128-dim for Kokoro)
    float default_speed = 1.0f;                  // Default speaking speed
    float default_pitch = 1.0f;                  // Default pitch adjustment

    // Optional metadata
    std::optional<std::string> description;
    std::optional<std::string> preview_url;
};

// TTS synthesis request
struct TTSRequest {
    std::string text;                            // Input text (Japanese)
    std::string voice_id;                        // Voice to use
    float speed = 1.0f;                          // Speaking speed (0.5-2.0)
    float pitch = 1.0f;                          // Pitch adjustment (0.5-2.0)
    float volume = 1.0f;                         // Volume adjustment (0.0-1.0)
    AudioFormat format = AudioFormat::WAV_PCM16; // Output format
    Priority priority = Priority::NORMAL;        // Processing priority

    // Advanced options
    std::optional<std::string> ipa_phonemes;     // Pre-computed IPA phonemes
    std::optional<int> vocabulary_id;            // Pre-defined vocabulary ID
    bool use_cache = true;                       // Enable caching
    bool normalize_text = true;                  // Normalize input text
};

// Audio data container
struct AudioData {
    std::vector<float> samples;                  // Audio samples (normalized -1 to 1)
    int sample_rate = 24000;                     // Sample rate (Hz)
    int channels = 1;                            // Number of channels (mono)
    std::chrono::milliseconds duration{0};       // Audio duration

    // Get size in bytes for specific format
    size_t GetSizeInBytes(AudioFormat format) const {
        size_t sample_size = (format == AudioFormat::WAV_PCM16 ||
                             format == AudioFormat::RAW_PCM16) ? 2 : 4;
        return samples.size() * sample_size;
    }

    // Convert to PCM16
    std::vector<int16_t> ToPCM16() const {
        std::vector<int16_t> pcm;
        pcm.reserve(samples.size());
        for (float sample : samples) {
            // Clamp and convert to int16
            float clamped = std::max(-1.0f, std::min(1.0f, sample));
            pcm.push_back(static_cast<int16_t>(clamped * 32767.0f));
        }
        return pcm;
    }
};

// Phoneme information
struct PhonemeInfo {
    std::string phoneme;                         // IPA phoneme symbol
    float duration = 0.0f;                       // Duration in seconds
    float stress = 0.0f;                         // Stress level (0-1)
    int position = 0;                            // Position in word
};

// Token information for debugging
struct TokenInfo {
    int token_id;                                // Token ID from vocabulary
    std::string phoneme;                         // Corresponding phoneme
    int position;                                // Position in sequence
};

// Processing statistics
struct ProcessingStats {
    std::chrono::milliseconds total_time{0};     // Total processing time
    std::chrono::milliseconds phonemization_time{0}; // Time for G2P conversion
    std::chrono::milliseconds tokenization_time{0};  // Time for tokenization
    std::chrono::milliseconds inference_time{0};     // ONNX inference time
    std::chrono::milliseconds audio_processing_time{0}; // Audio post-processing

    size_t text_length = 0;                      // Input text length
    size_t phoneme_count = 0;                    // Number of phonemes
    size_t token_count = 0;                      // Number of tokens
    size_t audio_samples = 0;                    // Number of audio samples

    bool cache_hit = false;                      // Whether cache was used
    int queue_position = 0;                      // Position in processing queue
};

// TTS result with metadata
struct TTSResult {
    Status status = Status::OK;                  // Operation status
    AudioData audio;                             // Generated audio data
    std::vector<PhonemeInfo> phonemes;           // Phoneme breakdown
    std::vector<TokenInfo> tokens;               // Token sequence (for debugging)
    ProcessingStats stats;                       // Performance statistics
    std::string error_message;                   // Error description if failed

    // Convenience methods
    bool IsSuccess() const { return status == Status::OK; }
    bool HasAudio() const { return !audio.samples.empty(); }
};

// Cache entry
struct CacheEntry {
    std::string key;                             // Cache key (hash of input + params)
    AudioData audio;                             // Cached audio
    std::vector<PhonemeInfo> phonemes;           // Cached phonemes
    std::chrono::system_clock::time_point created; // Creation time
    std::chrono::system_clock::time_point last_accessed; // Last access time
    size_t access_count = 0;                     // Number of accesses

    // Calculate age in seconds
    int64_t GetAgeSeconds() const {
        auto now = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(now - created).count();
    }
};

// Configuration for TTS engine
struct TTSConfig {
    // Model paths
    std::string kokoro_model_path = "models/kokoro-v1.0.int8.onnx";
    std::string phonemizer_model_path = "models/phonemizer.onnx";
    std::string dictionary_path = "data/ja_phonemes.json";
    std::string tokenizer_vocab_path = "models/tokenizer_vocab.json";
    std::string voices_dir = "models/voices";

    // Performance settings
    int max_concurrent_requests = 4;             // Max parallel synthesis
    int onnx_inter_threads = 0;                  // 0 = auto (all cores)
    int onnx_intra_threads = 0;                  // 0 = auto
    bool enable_gpu = false;                     // Use GPU if available

    // Cache settings
    bool enable_cache = true;                    // Enable result caching
    size_t max_cache_size_mb = 100;              // Max cache size in MB
    size_t max_cache_entries = 1000;             // Max number of entries
    int cache_ttl_seconds = 3600;                // Cache time-to-live

    // Audio settings
    int target_sample_rate = 24000;              // Output sample rate
    bool normalize_audio = true;                 // Normalize output level
    float silence_threshold = 0.01f;             // Silence detection threshold

    // Text processing
    bool enable_mecab = true;                    // Use MeCab for segmentation
    bool normalize_numbers = true;               // Convert numbers to words
    bool expand_abbreviations = true;            // Expand common abbreviations

    // Debug settings
    bool verbose = false;                        // Enable verbose logging
    bool save_intermediate = false;              // Save intermediate results
    std::string debug_output_dir = "debug";      // Debug output directory
};

// Callback function types
using ProgressCallback = std::function<void(float progress, const std::string& stage)>;
using ErrorCallback = std::function<void(Status status, const std::string& message)>;
using AudioCallback = std::function<void(const AudioData& audio)>;

} // namespace jp_edge_tts

#endif // JP_EDGE_TTS_TYPES_H