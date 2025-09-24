/**
 * @file vocab_manager.cpp
 * @brief Implementation of vocabulary management for tokenization
 * @author D Everett Hinton
 * @date 2025
 *
 * @copyright MIT License
 */

#include "jp_edge_tts/tokenizer/vocab_manager.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <algorithm>

using json = nlohmann::json;

namespace jp_edge_tts {

// ==========================================
// Private Implementation
// ==========================================

class VocabManager::Impl {
public:
    std::unordered_map<std::string, int> phoneme_to_id;
    std::unordered_map<int, std::string> id_to_phoneme;
    int next_id = 0;

    // Special token IDs
    int unk_token_id = -1;  // Unknown token

    bool LoadFromJSON(const std::string& json_str) {
        try {
            json data = json::parse(json_str);

            // Clear existing vocabulary
            phoneme_to_id.clear();
            id_to_phoneme.clear();
            next_id = 0;

            // Load vocabulary entries
            if (data.is_object()) {
                for (auto& [phoneme, id] : data.items()) {
                    if (id.is_number_integer()) {
                        int token_id = id.get<int>();
                        phoneme_to_id[phoneme] = token_id;
                        id_to_phoneme[token_id] = phoneme;
                        next_id = std::max(next_id, token_id + 1);

                        // Check for special tokens
                        if (phoneme == "<unk>" || phoneme == "[UNK]") {
                            unk_token_id = token_id;
                        }
                    }
                }
            }

            return true;

        } catch (const std::exception& e) {
            std::cerr << "Error parsing vocabulary JSON: " << e.what() << std::endl;
            return false;
        }
    }

    int GetTokenId(const std::string& phoneme) const {
        auto it = phoneme_to_id.find(phoneme);
        if (it != phoneme_to_id.end()) {
            return it->second;
        }

        // Return unknown token ID if available, otherwise -1
        return unk_token_id >= 0 ? unk_token_id : -1;
    }

    std::string GetPhoneme(int id) const {
        auto it = id_to_phoneme.find(id);
        if (it != id_to_phoneme.end()) {
            return it->second;
        }
        return "";
    }

    size_t GetSize() const {
        return phoneme_to_id.size();
    }

    bool HasPhoneme(const std::string& phoneme) const {
        return phoneme_to_id.find(phoneme) != phoneme_to_id.end();
    }

    void AddPhoneme(const std::string& phoneme) {
        if (!HasPhoneme(phoneme)) {
            int id = next_id++;
            phoneme_to_id[phoneme] = id;
            id_to_phoneme[id] = phoneme;
        }
    }

    void Clear() {
        phoneme_to_id.clear();
        id_to_phoneme.clear();
        next_id = 0;
        unk_token_id = -1;
    }
};

// ==========================================
// Public Interface Implementation
// ==========================================

VocabManager::VocabManager() : pImpl(std::make_unique<Impl>()) {}
VocabManager::~VocabManager() = default;

bool VocabManager::LoadFromFile(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "Failed to open vocabulary file: " << path << std::endl;
            return false;
        }

        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());

        return LoadFromJSON(content);

    } catch (const std::exception& e) {
        std::cerr << "Error loading vocabulary: " << e.what() << std::endl;
        return false;
    }
}

bool VocabManager::LoadFromJSON(const std::string& json) {
    return pImpl->LoadFromJSON(json);
}

int VocabManager::GetTokenId(const std::string& phoneme) const {
    return pImpl->GetTokenId(phoneme);
}

std::string VocabManager::GetPhoneme(int id) const {
    return pImpl->GetPhoneme(id);
}

size_t VocabManager::Size() const {
    return pImpl->GetSize();
}

bool VocabManager::Has(const std::string& phoneme) const {
    return pImpl->HasPhoneme(phoneme);
}

} // namespace jp_edge_tts