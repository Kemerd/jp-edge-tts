/**
 * @file mecab_wrapper.h
 * @brief MeCab wrapper for Japanese text segmentation and tokenization
 * @author JP Edge TTS Project
 * @date 2024
 *
 * @details Provides a C++ wrapper around MeCab for Japanese morphological
 * analysis, word segmentation, and reading generation. This tokenizer is
 * used as the primary method for breaking down Japanese text before
 * phonemization.
 *
 * @copyright MIT License
 */

#ifndef JP_EDGE_TTS_MECAB_WRAPPER_H
#define JP_EDGE_TTS_MECAB_WRAPPER_H

#include <string>
#include <vector>
#include <memory>
#include <optional>

namespace jp_edge_tts {

/**
 * @struct MorphemeInfo
 * @brief Information about a Japanese morpheme (word unit)
 */
struct MorphemeInfo {
    std::string surface;      ///< Surface form (original text)
    std::string reading;      ///< Reading in Katakana
    std::string pronunciation; ///< Pronunciation (may differ from reading)
    std::string pos;          ///< Part of speech
    std::string base_form;    ///< Dictionary/base form

    /**
     * @brief Check if this is a particle
     */
    bool IsParticle() const {
        return pos.find("助詞") != std::string::npos;
    }

    /**
     * @brief Check if this is punctuation
     */
    bool IsPunctuation() const {
        return pos.find("記号") != std::string::npos;
    }
};

/**
 * @class MeCabWrapper
 * @brief Wrapper class for MeCab Japanese morphological analyzer
 *
 * @details This class provides a clean C++ interface to MeCab, handling
 * initialization, text parsing, and resource management. It's used as
 * the primary tokenizer for Japanese text in the TTS pipeline.
 *
 * @note Thread-safe for parsing operations after initialization
 */
class MeCabWrapper {
public:
    /**
     * @brief Configuration for MeCab
     */
    struct Config {
        std::string dic_dir;      ///< Dictionary directory path
        std::string user_dic;     ///< Optional user dictionary path
        bool use_reading = true;  ///< Extract reading information
        bool normalize = true;    ///< Normalize text before processing
    };

    /**
     * @brief Constructor
     * @param config MeCab configuration
     */
    explicit MeCabWrapper(const Config& config = Config{});

    /**
     * @brief Destructor
     */
    ~MeCabWrapper();

    // Disable copy, enable move
    MeCabWrapper(const MeCabWrapper&) = delete;
    MeCabWrapper& operator=(const MeCabWrapper&) = delete;
    MeCabWrapper(MeCabWrapper&&) noexcept;
    MeCabWrapper& operator=(MeCabWrapper&&) noexcept;

    /**
     * @brief Initialize MeCab with dictionary
     *
     * @return true if successful, false otherwise
     *
     * @note Must be called before any parsing operations
     */
    bool Initialize();

    /**
     * @brief Check if initialized
     * @return true if ready to parse
     */
    bool IsInitialized() const;

    /**
     * @brief Parse Japanese text into morphemes
     *
     * @param text Input Japanese text
     * @return Vector of morpheme information
     *
     * @details Performs morphological analysis on the input text,
     * breaking it down into individual morphemes with readings
     * and part-of-speech information.
     *
     * @example
     * @code
     * auto morphemes = mecab.Parse("今日は晴れです");
     * // Returns morphemes for: 今日/は/晴れ/です
     * @endcode
     */
    std::vector<MorphemeInfo> Parse(const std::string& text);

    /**
     * @brief Tokenize text into surface forms only
     *
     * @param text Input Japanese text
     * @return Vector of surface form strings
     *
     * @details Simple tokenization returning only the surface forms
     * without additional linguistic information.
     */
    std::vector<std::string> Tokenize(const std::string& text);

    /**
     * @brief Get reading for Japanese text
     *
     * @param text Input Japanese text
     * @return Katakana reading of the text
     *
     * @details Converts the entire text to its Katakana reading,
     * useful for phoneme generation.
     *
     * @example
     * @code
     * auto reading = mecab.GetReading("漢字");
     * // Returns: "カンジ"
     * @endcode
     */
    std::string GetReading(const std::string& text);

    /**
     * @brief Get readings for each morpheme
     *
     * @param text Input Japanese text
     * @return Vector of reading strings
     */
    std::vector<std::string> GetReadings(const std::string& text);

    /**
     * @brief Convert Katakana reading to Hiragana
     *
     * @param katakana Katakana string
     * @return Hiragana string
     *
     * @details Utility function to convert Katakana to Hiragana,
     * useful for certain phonemization approaches.
     */
    static std::string KatakanaToHiragana(const std::string& katakana);

    /**
     * @brief Convert Hiragana to Katakana
     *
     * @param hiragana Hiragana string
     * @return Katakana string
     */
    static std::string HiraganaToKatakana(const std::string& hiragana);

    /**
     * @brief Normalize Japanese text
     *
     * @param text Input text
     * @return Normalized text
     *
     * @details Normalizes full-width characters, numbers, and
     * special symbols for consistent processing.
     */
    static std::string NormalizeText(const std::string& text);

    /**
     * @brief Check if text contains Kanji
     *
     * @param text Input text
     * @return true if text contains Kanji characters
     */
    static bool ContainsKanji(const std::string& text);

    /**
     * @brief Check if text is pure Hiragana
     *
     * @param text Input text
     * @return true if text is only Hiragana
     */
    static bool IsPureHiragana(const std::string& text);

    /**
     * @brief Check if text is pure Katakana
     *
     * @param text Input text
     * @return true if text is only Katakana
     */
    static bool IsPureKatakana(const std::string& text);

    /**
     * @brief Get MeCab version
     * @return Version string
     */
    std::string GetVersion() const;

    /**
     * @brief Get dictionary information
     * @return Dictionary info string
     */
    std::string GetDictionaryInfo() const;

    /**
     * @brief Add user dictionary
     *
     * @param path Path to user dictionary file
     * @return true if successfully added
     *
     * @note User dictionaries allow custom word definitions
     */
    bool AddUserDictionary(const std::string& path);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

/**
 * @brief Create a MeCab wrapper with default configuration
 * @return Unique pointer to MeCabWrapper
 */
std::unique_ptr<MeCabWrapper> CreateMeCabTokenizer();

/**
 * @brief Create a MeCab wrapper with custom configuration
 * @param config Configuration settings
 * @return Unique pointer to MeCabWrapper
 */
std::unique_ptr<MeCabWrapper> CreateMeCabTokenizer(const MeCabWrapper::Config& config);

} // namespace jp_edge_tts

#endif // JP_EDGE_TTS_MECAB_WRAPPER_H