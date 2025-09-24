/**
 * @file ipa_tokenizer.cpp
 * @brief Implementation of IPA phoneme tokenization
 * @author D Everett Hinton
 * @date 2025
 *
 * @copyright MIT License
 */

#include "jp_edge_tts/tokenizer/ipa_tokenizer.h"
#include <algorithm>
#include <sstream>
#include <cctype>
#include <fstream>
#include <iostream>

namespace jp_edge_tts {

// ==========================================
// Private Implementation
// ==========================================

class IPATokenizer::Impl {
public:
    std::unordered_map<std::string, int> phoneme_to_id;
    std::unordered_map<int, std::string> id_to_phoneme;
    bool is_loaded;

    // Special tokens
    IPATokenizer::SpecialTokens special_tokens;

    Impl() : is_loaded(false) {
        // Set default special token IDs
        special_tokens.pad_token = 0;
        special_tokens.unk_token = 1;
        special_tokens.start_token = 2;
        special_tokens.end_token = 3;
    }

    std::vector<std::string> SplitPhonemes(const std::string& phonemes) {
        std::vector<std::string> result;
        std::istringstream iss(phonemes);
        std::string phoneme;

        while (iss >> phoneme) {
            if (!phoneme.empty()) {
                result.push_back(phoneme);
            }
        }

        return result;
    }

    std::string NormalizePhoneme(const std::string& phoneme) {
        // Basic normalization - trim whitespace and lowercase
        std::string normalized = phoneme;

        // Trim whitespace
        auto start = normalized.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        auto end = normalized.find_last_not_of(" \t\r\n");
        normalized = normalized.substr(start, end - start + 1);

        return normalized;
    }

    bool LoadFromJSON(const std::string& json_data) {
        // Very basic JSON parsing for phoneme vocabulary
        // Expected format: {"phoneme1": id1, "phoneme2": id2, ...}

        phoneme_to_id.clear();
        id_to_phoneme.clear();

        // Simple JSON parsing - find all key-value pairs
        size_t pos = 0;
        while ((pos = json_data.find("\"", pos)) != std::string::npos) {
            size_t key_start = pos + 1;
            size_t key_end = json_data.find("\"", key_start);
            if (key_end == std::string::npos) break;

            std::string key = json_data.substr(key_start, key_end - key_start);

            // Find the colon and value
            size_t colon = json_data.find(":", key_end);
            if (colon == std::string::npos) break;

            size_t value_start = json_data.find_first_of("0123456789", colon);
            if (value_start == std::string::npos) break;

            size_t value_end = json_data.find_first_not_of("0123456789", value_start);
            if (value_end == std::string::npos) value_end = json_data.length();

            int value = std::stoi(json_data.substr(value_start, value_end - value_start));

            phoneme_to_id[key] = value;
            id_to_phoneme[value] = key;

            pos = value_end;
        }

        is_loaded = !phoneme_to_id.empty();
        return is_loaded;
    }
};

// ==========================================
// Public Interface Implementation
// ==========================================

IPATokenizer::IPATokenizer() : pImpl(std::make_unique<Impl>()) {}
IPATokenizer::~IPATokenizer() = default;
IPATokenizer::IPATokenizer(IPATokenizer&&) noexcept = default;
IPATokenizer& IPATokenizer::operator=(IPATokenizer&&) noexcept = default;

bool IPATokenizer::LoadVocabulary(const std::string& vocab_path) {
    std::ifstream file(vocab_path);
    if (!file.is_open()) {
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    return LoadVocabularyFromJSON(content);
}

bool IPATokenizer::LoadVocabularyFromJSON(const std::string& json_data) {
    return pImpl->LoadFromJSON(json_data);
}

bool IPATokenizer::IsLoaded() const {
    return pImpl->is_loaded;
}

std::vector<int> IPATokenizer::PhonemesToTokens(const std::string& phonemes) {
    if (!pImpl->is_loaded) {
        return {};
    }

    auto phoneme_list = pImpl->SplitPhonemes(phonemes);
    return PhonemesToTokens(phoneme_list);
}

std::vector<int> IPATokenizer::PhonemesToTokens(const std::vector<std::string>& phoneme_list) {
    if (!pImpl->is_loaded) {
        return {};
    }

    std::vector<int> tokens;
    tokens.reserve(phoneme_list.size());

    for (const auto& phoneme : phoneme_list) {
        std::string normalized = pImpl->NormalizePhoneme(phoneme);

        auto it = pImpl->phoneme_to_id.find(normalized);
        if (it != pImpl->phoneme_to_id.end()) {
            tokens.push_back(it->second);
        } else {
            tokens.push_back(pImpl->special_tokens.unk_token);
        }
    }

    return tokens;
}

std::string IPATokenizer::TokensToPhonemes(const std::vector<int>& tokens) {
    if (!pImpl->is_loaded) {
        return "";
    }

    std::vector<std::string> phonemes;
    phonemes.reserve(tokens.size());

    for (int token_id : tokens) {
        auto it = pImpl->id_to_phoneme.find(token_id);
        if (it != pImpl->id_to_phoneme.end()) {
            phonemes.push_back(it->second);
        } else {
            // Use unknown token representation
            phonemes.push_back("<unk>");
        }
    }

    // Join with spaces
    std::ostringstream oss;
    for (size_t i = 0; i < phonemes.size(); ++i) {
        if (i > 0) oss << " ";
        oss << phonemes[i];
    }

    return oss.str();
}

int IPATokenizer::GetTokenId(const std::string& phoneme) const {
    if (!pImpl->is_loaded) {
        return pImpl->special_tokens.unk_token;
    }

    std::string normalized = pImpl->NormalizePhoneme(phoneme);
    auto it = pImpl->phoneme_to_id.find(normalized);

    if (it != pImpl->phoneme_to_id.end()) {
        return it->second;
    }

    return pImpl->special_tokens.unk_token;
}

std::string IPATokenizer::GetPhoneme(int token_id) const {
    if (!pImpl->is_loaded) {
        return "";
    }

    auto it = pImpl->id_to_phoneme.find(token_id);
    if (it != pImpl->id_to_phoneme.end()) {
        return it->second;
    }

    return "";
}

size_t IPATokenizer::GetVocabularySize() const {
    return pImpl->phoneme_to_id.size();
}

std::vector<std::string> IPATokenizer::GetPhonemes() const {
    std::vector<std::string> phonemes;
    phonemes.reserve(pImpl->phoneme_to_id.size());

    for (const auto& pair : pImpl->phoneme_to_id) {
        phonemes.push_back(pair.first);
    }

    std::sort(phonemes.begin(), phonemes.end());
    return phonemes;
}

IPATokenizer::SpecialTokens IPATokenizer::GetSpecialTokens() const {
    return pImpl->special_tokens;
}

std::vector<int> IPATokenizer::PadTokens(const std::vector<int>& tokens,
                                         size_t target_length,
                                         bool pad_left) {
    if (tokens.size() >= target_length) {
        return tokens;
    }

    std::vector<int> padded;
    padded.reserve(target_length);

    size_t padding_needed = target_length - tokens.size();

    if (pad_left) {
        // Pad on the left
        for (size_t i = 0; i < padding_needed; ++i) {
            padded.push_back(pImpl->special_tokens.pad_token);
        }
        padded.insert(padded.end(), tokens.begin(), tokens.end());
    } else {
        // Pad on the right
        padded.insert(padded.end(), tokens.begin(), tokens.end());
        for (size_t i = 0; i < padding_needed; ++i) {
            padded.push_back(pImpl->special_tokens.pad_token);
        }
    }

    return padded;
}

std::vector<int> IPATokenizer::TruncateTokens(const std::vector<int>& tokens,
                                              size_t max_length) {
    if (tokens.size() <= max_length) {
        return tokens;
    }

    return std::vector<int>(tokens.begin(), tokens.begin() + max_length);
}

} // namespace jp_edge_tts