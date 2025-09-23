/**
 * @file ipa_tokenizer.h
 * @brief IPA phoneme to token ID conversion
 * D Everett Hinton
 * @date 2025
 *
 * @copyright MIT License
 */

#ifndef JP_EDGE_TTS_IPA_TOKENIZER_H
#define JP_EDGE_TTS_IPA_TOKENIZER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace jp_edge_tts {

/**
 * @class IPATokenizer
 * @brief Converts IPA phonemes to token IDs for model input
 *
 * @details Maps IPA phoneme symbols to integer token IDs based on
 * the vocabulary used to train the TTS model.
 */
class IPATokenizer {
public:
    /**
     * @brief Constructor
     */
    IPATokenizer();

    /**
     * @brief Destructor
     */
    ~IPATokenizer();

    // Disable copy, enable move
    IPATokenizer(const IPATokenizer&) = delete;
    IPATokenizer& operator=(const IPATokenizer&) = delete;
    IPATokenizer(IPATokenizer&&) noexcept;
    IPATokenizer& operator=(IPATokenizer&&) noexcept;

    /**
     * @brief Load vocabulary from JSON file
     *
     * @param vocab_path Path to vocabulary JSON file
     * @return true if successful
     */
    bool LoadVocabulary(const std::string& vocab_path);

    /**
     * @brief Load vocabulary from JSON string
     *
     * @param json_data Vocabulary as JSON string
     * @return true if successful
     */
    bool LoadVocabularyFromJSON(const std::string& json_data);

    /**
     * @brief Check if vocabulary is loaded
     * @return true if ready to tokenize
     */
    bool IsLoaded() const;

    /**
     * @brief Convert IPA phonemes to token IDs
     *
     * @param phonemes Space-separated IPA phonemes
     * @return Vector of token IDs
     */
    std::vector<int> PhonemesToTokens(const std::string& phonemes);

    /**
     * @brief Convert phoneme list to token IDs
     *
     * @param phoneme_list Vector of phoneme strings
     * @return Vector of token IDs
     */
    std::vector<int> PhonemesToTokens(const std::vector<std::string>& phoneme_list);

    /**
     * @brief Convert tokens back to phonemes
     *
     * @param tokens Vector of token IDs
     * @return Space-separated phoneme string
     */
    std::string TokensToPhonemes(const std::vector<int>& tokens);

    /**
     * @brief Get token ID for phoneme
     *
     * @param phoneme IPA phoneme symbol
     * @return Token ID, or unknown token ID if not found
     */
    int GetTokenId(const std::string& phoneme) const;

    /**
     * @brief Get phoneme for token ID
     *
     * @param token_id Token ID
     * @return Phoneme string, or empty if invalid
     */
    std::string GetPhoneme(int token_id) const;

    /**
     * @brief Get vocabulary size
     * @return Number of tokens in vocabulary
     */
    size_t GetVocabularySize() const;

    /**
     * @brief Get all phonemes in vocabulary
     * @return Vector of phoneme strings
     */
    std::vector<std::string> GetPhonemes() const;

    /**
     * @brief Get special token IDs
     */
    struct SpecialTokens {
        int pad_token = 0;
        int start_token = 0;
        int end_token = 0;
        int unk_token = 1;
    };
    SpecialTokens GetSpecialTokens() const;

    /**
     * @brief Add padding to token sequence
     *
     * @param tokens Input tokens
     * @param target_length Desired sequence length
     * @param pad_left Pad on left side if true
     * @return Padded token sequence
     */
    std::vector<int> PadTokens(const std::vector<int>& tokens,
                               size_t target_length,
                               bool pad_left = false);

    /**
     * @brief Truncate token sequence
     *
     * @param tokens Input tokens
     * @param max_length Maximum sequence length
     * @return Truncated token sequence
     */
    std::vector<int> TruncateTokens(const std::vector<int>& tokens,
                                    size_t max_length);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace jp_edge_tts

#endif // JP_EDGE_TTS_IPA_TOKENIZER_H