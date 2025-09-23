/**
 * @file phonemizer.h
 * @brief Japanese text to IPA phoneme conversion interface
 * D Everett Hinton
 * @date 2025
 *
 * @details This file defines the JapanesePhonemizer class which handles
 * grapheme-to-phoneme (G2P) conversion for Japanese text using a hybrid
 * approach: dictionary lookup, DeepPhonemizer ONNX model, and fallback rules.
 *
 * @copyright MIT License
 */

#ifndef JP_EDGE_TTS_PHONEMIZER_H
#define JP_EDGE_TTS_PHONEMIZER_H

#include "types.h"
#include <unordered_map>
#include <memory>
#include <optional>
#include <vector>
#include <string>

namespace jp_edge_tts {

// Forward declarations
class OnnxSession;

/**
 * @class JapanesePhonemizer
 * @brief Converts Japanese text to IPA phoneme representation
 *
 * @details The JapanesePhonemizer implements a hierarchical approach:
 * 1. Dictionary lookup for known words/phrases (fastest)
 * 2. DeepPhonemizer ONNX model for unknown words (accurate)
 * 3. Rule-based fallback for edge cases (reliable)
 *
 * It handles:
 * - Mixed scripts (Hiragana, Katakana, Kanji, Romaji)
 * - Numbers and special characters
 * - Punctuation and prosody markers
 *
 * @note Thread-safe for concurrent phonemization operations
 */
class JapanesePhonemizer {
public:
    /**
     * @brief Configuration for the phonemizer
     */
    struct Config {
        std::string dictionary_path = "data/ja_phonemes.json";  ///< Path to phoneme dictionary
        std::string onnx_model_path = "models/phonemizer.onnx"; ///< Path to ONNX model
        bool enable_cache = true;                                ///< Enable phoneme caching
        size_t max_cache_size = 10000;                          ///< Maximum cache entries
        bool use_mecab = true;                                  ///< Use MeCab for segmentation
        bool normalize_text = true;                             ///< Normalize input text
    };

    /**
     * @brief Construct a Japanese phonemizer
     * @param config Configuration settings
     */
    explicit JapanesePhonemizer(const Config& config = Config{});

    /**
     * @brief Destructor
     */
    ~JapanesePhonemizer();

    // Disable copy, enable move
    JapanesePhonemizer(const JapanesePhonemizer&) = delete;
    JapanesePhonemizer& operator=(const JapanesePhonemizer&) = delete;
    JapanesePhonemizer(JapanesePhonemizer&&) noexcept;
    JapanesePhonemizer& operator=(JapanesePhonemizer&&) noexcept;

    /**
     * @brief Initialize the phonemizer
     *
     * @details Loads dictionary, initializes ONNX model, and sets up MeCab
     * @return Status::OK if successful
     *
     * @note This must be called before any phonemization operations
     */
    Status Initialize();

    /**
     * @brief Check if phonemizer is initialized
     * @return true if ready for phonemization
     */
    bool IsInitialized() const;

    /**
     * @brief Convert Japanese text to IPA phonemes
     *
     * @param text Input Japanese text
     * @return IPA phoneme string with space separators
     *
     * @details Main phonemization method that applies the full pipeline:
     * segmentation, normalization, and G2P conversion.
     *
     * @example
     * @code
     * auto phonemes = phonemizer.Phonemize("こんにちは");
     * // Returns: "k o ɴ n i tɕ i w a"
     * @endcode
     */
    std::string Phonemize(const std::string& text);

    /**
     * @brief Convert text to phonemes with detailed information
     *
     * @param text Input Japanese text
     * @return Vector of phoneme information including timing hints
     *
     * @details Provides detailed phoneme information for advanced use cases
     */
    std::vector<PhonemeInfo> PhonemizeDetailed(const std::string& text);

    /**
     * @brief Batch phonemization for multiple texts
     *
     * @param texts Vector of Japanese texts
     * @return Vector of IPA phoneme strings
     *
     * @details Optimized for processing multiple texts efficiently
     */
    std::vector<std::string> PhonemizeBatch(const std::vector<std::string>& texts);

    /**
     * @brief Segment Japanese text into words/morphemes
     *
     * @param text Input Japanese text
     * @return Vector of segmented words
     *
     * @details Uses MeCab if available, otherwise falls back to character-based
     *
     * @example
     * @code
     * auto segments = phonemizer.SegmentText("私は学生です");
     * // Returns: ["私", "は", "学生", "です"]
     * @endcode
     */
    std::vector<std::string> SegmentText(const std::string& text);

    /**
     * @brief Normalize Japanese text for phonemization
     *
     * @param text Input text
     * @return Normalized text
     *
     * @details Handles:
     * - Number to word conversion
     * - Full-width to half-width conversion
     * - Special character normalization
     */
    std::string NormalizeText(const std::string& text);

    /**
     * @brief Look up word in phoneme dictionary
     *
     * @param word Japanese word or phrase
     * @return IPA phonemes if found, nullopt otherwise
     *
     * @note Dictionary lookups are case-sensitive
     */
    std::optional<std::string> LookupDictionary(const std::string& word) const;

    /**
     * @brief Add custom word to phoneme dictionary
     *
     * @param word Japanese word or phrase
     * @param phonemes IPA phoneme representation
     * @return true if successfully added
     *
     * @details Useful for adding domain-specific vocabulary
     *
     * @example
     * @code
     * phonemizer.AddToDictionary("固有名詞", "k o j u u m e i ɕ i");
     * @endcode
     */
    bool AddToDictionary(const std::string& word, const std::string& phonemes);

    /**
     * @brief Remove word from dictionary
     *
     * @param word Word to remove
     * @return true if word was removed
     */
    bool RemoveFromDictionary(const std::string& word);

    /**
     * @brief Load additional dictionary from file
     *
     * @param path Path to JSON dictionary file
     * @return Status::OK if successful
     *
     * @details Dictionary format: {"word": "phonemes", ...}
     */
    Status LoadAdditionalDictionary(const std::string& path);

    /**
     * @brief Export current dictionary to file
     *
     * @param path Output path for JSON file
     * @param include_learned Include dynamically learned entries
     * @return Status::OK if successful
     */
    Status ExportDictionary(const std::string& path, bool include_learned = true);

    /**
     * @brief Clear phoneme cache
     *
     * @details Removes all cached phonemization results
     */
    void ClearCache();

    /**
     * @brief Get cache statistics
     *
     * @return Cache hit rate, size, and other metrics
     */
    struct CacheStats {
        size_t total_entries;    ///< Number of cached entries
        size_t memory_bytes;     ///< Memory used by cache
        size_t hit_count;        ///< Number of cache hits
        size_t miss_count;       ///< Number of cache misses
        float hit_rate;          ///< Cache hit rate (0-1)
    };
    CacheStats GetCacheStats() const;

    /**
     * @brief Set maximum cache size
     *
     * @param max_entries Maximum number of cache entries
     * @note Least recently used entries are evicted when limit is reached
     */
    void SetMaxCacheSize(size_t max_entries);

    /**
     * @brief Get supported phoneme set
     *
     * @return Set of all IPA phonemes used by this phonemizer
     */
    std::vector<std::string> GetPhonemeSet() const;

    /**
     * @brief Enable or disable specific features
     */
    void EnableMeCab(bool enable);
    void EnableCache(bool enable);
    void EnableNormalization(bool enable);

    /**
     * @brief Warm up the phonemizer with sample text
     *
     * @details Performs dummy phonemization to initialize models
     * @return Status::OK if successful
     */
    Status Warmup();

private:
    /**
     * @brief Private implementation
     */
    class Impl;
    std::unique_ptr<Impl> pImpl;

    // Internal methods
    std::string PhonemizeWithModel(const std::string& text);
    std::string PhonemizeWithRules(const std::string& text);
    std::string ApplyPostProcessing(const std::string& phonemes);
    bool IsKanji(const std::string& text) const;
    bool IsHiragana(const std::string& text) const;
    bool IsKatakana(const std::string& text) const;
};

/**
 * @brief Factory function to create a phonemizer with default settings
 * @return Unique pointer to JapanesePhonemizer instance
 */
std::unique_ptr<JapanesePhonemizer> CreatePhonemizer();

/**
 * @brief Factory function to create a phonemizer with custom config
 * @param config Configuration settings
 * @return Unique pointer to JapanesePhonemizer instance
 */
std::unique_ptr<JapanesePhonemizer> CreatePhonemizer(
    const JapanesePhonemizer::Config& config);

/**
 * @brief Convert Romaji to IPA phonemes
 * @param romaji Input Romaji text
 * @return IPA phoneme representation
 */
std::string RomajiToPhonemes(const std::string& romaji);

/**
 * @brief Convert Hiragana to IPA phonemes
 * @param hiragana Input Hiragana text
 * @return IPA phoneme representation
 */
std::string HiraganaToPhonemes(const std::string& hiragana);

/**
 * @brief Convert Katakana to IPA phonemes
 * @param katakana Input Katakana text
 * @return IPA phoneme representation
 */
std::string KatakanaToPhonemes(const std::string& katakana);

} // namespace jp_edge_tts

#endif // JP_EDGE_TTS_PHONEMIZER_H