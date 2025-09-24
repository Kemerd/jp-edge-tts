/**
 * @file dictionary_lookup.cpp
 * @brief Implementation of dictionary-based phoneme lookup
 * @author D Everett Hinton
 * @date 2025
 *
 * @copyright MIT License
 */

#include "jp_edge_tts/phonemizer/dictionary_lookup.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <algorithm>

using json = nlohmann::json;

namespace jp_edge_tts {

// ==========================================
// Private Implementation
// ==========================================

class DictionaryLookup::Impl {
public:
    // Main dictionary: word -> phonemes
    std::unordered_map<std::string, std::string> dictionary;

    // Statistics
    size_t total_entries = 0;
    size_t lookup_hits = 0;
    size_t lookup_misses = 0;

    bool LoadFromJSON(const std::string& json_str) {
        try {
            json data = json::parse(json_str);

            // Clear existing dictionary
            dictionary.clear();
            total_entries = 0;

            // Load dictionary entries
            if (data.is_object()) {
                for (auto& [word, phonemes] : data.items()) {
                    if (phonemes.is_string()) {
                        dictionary[word] = phonemes.get<std::string>();
                        total_entries++;
                    }
                }
            }

            return true;

        } catch (const std::exception& e) {
            std::cerr << "Error parsing dictionary JSON: " << e.what() << std::endl;
            return false;
        }
    }

    std::optional<std::string> LookupWord(const std::string& word) const {
        auto it = dictionary.find(word);
        if (it != dictionary.end()) {
            return it->second;
        }

        // Try lowercase version
        std::string lower_word = word;
        std::transform(lower_word.begin(), lower_word.end(), lower_word.begin(), ::tolower);

        it = dictionary.find(lower_word);
        if (it != dictionary.end()) {
            return it->second;
        }

        return std::nullopt;
    }

    void AddEntry(const std::string& word, const std::string& phonemes) {
        bool is_new = dictionary.find(word) == dictionary.end();
        dictionary[word] = phonemes;

        if (is_new) {
            total_entries++;
        }
    }

    void ClearAll() {
        dictionary.clear();
        total_entries = 0;
        lookup_hits = 0;
        lookup_misses = 0;
    }

    bool HasWord(const std::string& word) const {
        return dictionary.find(word) != dictionary.end();
    }

    size_t GetSize() const {
        return total_entries;
    }
};

// ==========================================
// Public Interface Implementation
// ==========================================

DictionaryLookup::DictionaryLookup() : pImpl(std::make_unique<Impl>()) {}
DictionaryLookup::~DictionaryLookup() = default;

bool DictionaryLookup::LoadFromFile(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "Failed to open dictionary file: " << path << std::endl;
            return false;
        }

        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());

        return pImpl->LoadFromJSON(content);

    } catch (const std::exception& e) {
        std::cerr << "Error loading dictionary: " << e.what() << std::endl;
        return false;
    }
}

std::optional<std::string> DictionaryLookup::Lookup(const std::string& word) const {
    auto result = pImpl->LookupWord(word);

    if (result.has_value()) {
        pImpl->lookup_hits++;
    } else {
        pImpl->lookup_misses++;
    }

    return result;
}

void DictionaryLookup::Add(const std::string& word, const std::string& phonemes) {
    pImpl->AddEntry(word, phonemes);
}

size_t DictionaryLookup::Size() const {
    return pImpl->GetSize();
}

void DictionaryLookup::Clear() {
    pImpl->ClearAll();
}

bool DictionaryLookup::Has(const std::string& word) const {
    return pImpl->HasWord(word);
}

} // namespace jp_edge_tts