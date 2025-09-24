/**
 * @file tts_engine.h
 * @brief Main TTS Engine interface for Japanese text-to-speech synthesis
 * D Everett Hinton
 * @date 2025
 *
 * @details This file contains the main TTSEngine class which provides
 * a comprehensive API for Japanese text-to-speech synthesis using
 * ONNX Runtime, DeepPhonemizer, and the Kokoro TTS model.
 *
 * @copyright MIT License
 */

#ifndef JP_EDGE_TTS_ENGINE_H
#define JP_EDGE_TTS_ENGINE_H

#include "jp_edge_tts/types.h"
#include <memory>
#include <future>
#include <queue>
#include <mutex>
#include <atomic>

namespace jp_edge_tts {

// Forward declarations
class SessionManager;
class JapanesePhonemizer;
class IPATokenizer;
class AudioProcessor;
class CacheManager;
class ThreadPool;

/**
 * @class TTSEngine
 * @brief Main text-to-speech synthesis engine for Japanese
 *
 * @details The TTSEngine class provides a complete interface for converting
 * Japanese text to speech using advanced neural network models. It supports:
 * - Multiple voices with customizable parameters
 * - Synchronous and asynchronous synthesis
 * - Intelligent caching for improved performance
 * - Japanese-specific text processing with MeCab
 * - ONNX Runtime integration for efficient inference
 *
 * @note This class is thread-safe and supports concurrent synthesis operations.
 *
 * @example
 * @code
 * jp_edge_tts::TTSConfig config;
 * config.enable_gpu = true;
 *
 * auto engine = jp_edge_tts::CreateTTSEngine(config);
 * engine->Initialize();
 *
 * auto result = engine->SynthesizeSimple("こんにちは", "jf_alpha");
 * if (result.IsSuccess()) {
 *     engine->SaveAudioToFile(result.audio, "output.wav");
 * }
 * @endcode
 */
class TTSEngine {
public:
    /**
     * @brief Construct a new TTS Engine with optional configuration
     * @param config Configuration settings for the engine
     */
    explicit TTSEngine(const TTSConfig& config = TTSConfig{});

    /**
     * @brief Destroy the TTS Engine and release all resources
     */
    ~TTSEngine();

    // Disable copy construction and assignment
    TTSEngine(const TTSEngine&) = delete;
    TTSEngine& operator=(const TTSEngine&) = delete;

    // Enable move semantics
    TTSEngine(TTSEngine&&) noexcept;
    TTSEngine& operator=(TTSEngine&&) noexcept;

    // ==========================================
    // Initialization and Configuration
    // ==========================================

    /**
     * @brief Initialize the engine with current configuration
     *
     * @details This method loads all required models, initializes ONNX Runtime,
     * sets up the phonemizer, and prepares the cache. Must be called before
     * any synthesis operations.
     *
     * @return Status::OK if successful, error code otherwise
     *
     * @note This operation may take several seconds depending on model size
     *
     * @pre Engine must not be already initialized
     * @post Engine is ready for synthesis operations
     */
    Status Initialize();

    // Check if engine is initialized and ready
    bool IsInitialized() const;

    // Update configuration (some settings require re-initialization)
    Status UpdateConfig(const TTSConfig& config);

    // Get current configuration
    const TTSConfig& GetConfig() const;

    // ==========================================
    // Voice Management
    // ==========================================

    // Load a voice from file
    Status LoadVoice(const std::string& voice_path);

    // Load a voice from memory
    Status LoadVoiceFromMemory(const std::string& voice_id,
                               const std::vector<float>& style_vector);

    // Get list of available voices
    std::vector<Voice> GetAvailableVoices() const;

    // Get voice information
    std::optional<Voice> GetVoice(const std::string& voice_id) const;

    // Set default voice
    Status SetDefaultVoice(const std::string& voice_id);

    // Unload a voice to free memory
    Status UnloadVoice(const std::string& voice_id);

    // ==========================================
    // Synchronous TTS Synthesis
    // ==========================================

    /**
     * @brief Simple text-to-speech synthesis with default settings
     *
     * @param text Japanese text to synthesize
     * @param voice_id Voice identifier (uses default if empty)
     * @return TTSResult containing audio data and metadata
     *
     * @details This is a convenience method for simple TTS synthesis.
     * It uses default settings for speed, pitch, and volume.
     *
     * @example
     * @code
     * auto result = engine->SynthesizeSimple("今日はいい天気ですね");
     * @endcode
     */
    TTSResult SynthesizeSimple(const std::string& text,
                               const std::string& voice_id = "");

    // Full synthesis with all options
    TTSResult Synthesize(const TTSRequest& request);

    // Batch synthesis for multiple texts
    std::vector<TTSResult> SynthesizeBatch(const std::vector<TTSRequest>& requests);

    // ==========================================
    // Asynchronous TTS Synthesis
    // ==========================================

    // Async synthesis returning a future
    std::future<TTSResult> SynthesizeAsync(const TTSRequest& request);

    // Batch async synthesis
    std::vector<std::future<TTSResult>> SynthesizeBatchAsync(
        const std::vector<TTSRequest>& requests);

    // Submit request to queue (fire-and-forget)
    std::string SubmitRequest(const TTSRequest& request,
                             AudioCallback callback = nullptr);

    // Check request status
    bool IsRequestComplete(const std::string& request_id) const;

    // Cancel a pending request
    bool CancelRequest(const std::string& request_id);

    // ==========================================
    // Text Processing and Analysis
    // ==========================================

    /**
     * @brief Convert Japanese text to IPA phonemes
     *
     * @param text Input Japanese text
     * @return Vector of phoneme information including timing
     *
     * @details Uses dictionary lookup first, then DeepPhonemizer for
     * unknown words. Handles mixed scripts (Hiragana, Katakana, Kanji).
     *
     * @note This method is useful for debugging pronunciation issues
     */
    std::vector<PhonemeInfo> TextToPhonemes(const std::string& text);

    // Convert IPA phonemes to token IDs
    std::vector<int> PhonemesToTokens(const std::string& phonemes);

    // Full pipeline: text -> phonemes -> tokens (for debugging)
    std::pair<std::string, std::vector<int>> ProcessText(const std::string& text);

    // Normalize Japanese text (numbers, punctuation, etc.)
    std::string NormalizeText(const std::string& text);

    // Segment Japanese text using MeCab or fallback
    std::vector<std::string> SegmentText(const std::string& text);

    // ==========================================
    // Audio Output
    // ==========================================

    // Save audio to file
    Status SaveAudioToFile(const AudioData& audio,
                          const std::string& filepath,
                          AudioFormat format = AudioFormat::WAV_PCM16);

    // Convert audio format
    std::vector<uint8_t> ConvertAudioFormat(const AudioData& audio,
                                           AudioFormat format);

    // Get audio duration in milliseconds
    int GetAudioDuration(const AudioData& audio) const;

    // ==========================================
    // Cache Management
    // ==========================================

    // Clear all cached results
    void ClearCache();

    // Clear cache entries older than specified seconds
    void ClearCacheOlderThan(int seconds);

    // Get cache statistics
    struct CacheStats {
        size_t total_entries;
        size_t total_size_bytes;
        size_t hit_count;
        size_t miss_count;
        float hit_rate;
    };
    CacheStats GetCacheStats() const;

    // Preload cache with common phrases
    Status PreloadCache(const std::vector<TTSRequest>& requests);

    // ==========================================
    // Performance and Monitoring
    // ==========================================

    // Get current queue size
    size_t GetQueueSize() const;

    // Get number of active synthesis operations
    size_t GetActiveSynthesisCount() const;

    // Set progress callback for long operations
    void SetProgressCallback(ProgressCallback callback);

    // Set error callback
    void SetErrorCallback(ErrorCallback callback);

    // Get performance statistics
    struct PerformanceStats {
        size_t total_requests;
        size_t successful_requests;
        size_t failed_requests;
        std::chrono::milliseconds average_latency;
        std::chrono::milliseconds min_latency;
        std::chrono::milliseconds max_latency;
        float requests_per_second;
    };
    PerformanceStats GetPerformanceStats() const;

    // Reset performance counters
    void ResetPerformanceStats();

    // ==========================================
    // Advanced Features
    // ==========================================

    // Enable/disable specific processing stages (for debugging)
    void EnablePhonemization(bool enable);
    void EnableTokenization(bool enable);
    void EnableAudioNormalization(bool enable);

    // Set custom phoneme dictionary
    Status LoadCustomDictionary(const std::string& dict_path);

    // Add word to phoneme dictionary
    Status AddWordToDictionary(const std::string& word,
                              const std::string& phonemes);

    // Export current dictionary
    Status ExportDictionary(const std::string& output_path);

    // Warmup the engine with dummy inference
    Status Warmup();

    // ==========================================
    // Resource Management
    // ==========================================

    // Get memory usage in bytes
    size_t GetMemoryUsage() const;

    // Release unused resources
    void ReleaseUnusedResources();

    // Set memory limits
    void SetMaxMemoryUsage(size_t max_bytes);

    // Shutdown the engine gracefully
    void Shutdown();

private:
    // Internal implementation
    class Impl;
    std::unique_ptr<Impl> pImpl;

    // Internal methods
    TTSResult ProcessRequest(const TTSRequest& request);
    void ProcessQueue();
    std::string GenerateRequestId();
    std::string ComputeCacheKey(const TTSRequest& request);
};

// ==========================================
// Factory Functions
// ==========================================

// Create a TTS engine with default configuration
std::unique_ptr<TTSEngine> CreateTTSEngine();

// Create a TTS engine with custom configuration
std::unique_ptr<TTSEngine> CreateTTSEngine(const TTSConfig& config);

// Get version information
std::string GetVersion();

// Check if GPU is available
bool IsGPUAvailable();

// Get list of available ONNX providers
std::vector<std::string> GetAvailableProviders();

} // namespace jp_edge_tts

#endif // JP_EDGE_TTS_ENGINE_H