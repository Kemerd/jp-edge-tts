/**
 * @file vocab_manager.h
 * @brief Vocabulary management for tokenization
 * @author D Everett Hinton
 * @date 2025
 *
 * @copyright MIT License
 */

#ifndef JP_EDGE_TTS_VOCAB_MANAGER_H
#define JP_EDGE_TTS_VOCAB_MANAGER_H

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

namespace jp_edge_tts {

/**
 * @class VocabManager
 * @brief Manages phoneme to token ID vocabulary mapping
 */
class VocabManager {
public:
    VocabManager();
    ~VocabManager();

    /**
     * @brief Load vocabulary from JSON file
     * @param path Path to vocabulary JSON
     * @return true if successful
     */
    bool LoadFromFile(const std::string& path);

    /**
     * @brief Load vocabulary from JSON string
     * @param json JSON data
     * @return true if successful
     */
    bool LoadFromJSON(const std::string& json);

    /**
     * @brief Get token ID for phoneme
     * @param phoneme IPA phoneme
     * @return Token ID
     */
    int GetTokenId(const std::string& phoneme) const;

    /**
     * @brief Get phoneme for token ID
     * @param id Token ID
     * @return Phoneme string
     */
    std::string GetPhoneme(int id) const;

    /**
     * @brief Get vocabulary size
     * @return Number of tokens
     */
    size_t Size() const;

    /**
     * @brief Check if phoneme exists
     * @param phoneme IPA phoneme
     * @return true if exists
     */
    bool Has(const std::string& phoneme) const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace jp_edge_tts

#endif // JP_EDGE_TTS_VOCAB_MANAGER_H