/**
 * @file dictionary_lookup.h
 * @brief Dictionary-based phoneme lookup for Japanese
 * @author D Everett Hinton
 * @date 2025
 *
 * @copyright MIT License
 */

#ifndef JP_EDGE_TTS_DICTIONARY_LOOKUP_H
#define JP_EDGE_TTS_DICTIONARY_LOOKUP_H

#include <string>
#include <unordered_map>
#include <memory>
#include <optional>

namespace jp_edge_tts {

/**
 * @class DictionaryLookup
 * @brief Fast dictionary-based phoneme lookup
 */
class DictionaryLookup {
public:
    DictionaryLookup();
    ~DictionaryLookup();

    /**
     * @brief Load dictionary from JSON file
     * @param path Path to dictionary JSON
     * @return true if successful
     */
    bool LoadFromFile(const std::string& path);

    /**
     * @brief Lookup phonemes for word
     * @param word Japanese word
     * @return IPA phonemes if found
     */
    std::optional<std::string> Lookup(const std::string& word) const;

    /**
     * @brief Add word to dictionary
     * @param word Japanese word
     * @param phonemes IPA phonemes
     */
    void Add(const std::string& word, const std::string& phonemes);

    /**
     * @brief Get dictionary size
     * @return Number of entries
     */
    size_t Size() const;

    /**
     * @brief Clear dictionary
     */
    void Clear();

    /**
     * @brief Check if word exists
     * @param word Japanese word
     * @return true if exists
     */
    bool Has(const std::string& word) const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace jp_edge_tts

#endif // JP_EDGE_TTS_DICTIONARY_LOOKUP_H