/**
 * @file vocab_manager.cpp
 * @brief Implementation of vocabulary management for tokenization
 * @author D Everett Hinton
 * @date 2025
 *
 * @copyright MIT License
 */

#include "jp_edge_tts/tokenizer/vocab_manager.h"
#include "jp_edge_tts/utils/file_utils.h"
#include "jp_edge_tts/utils/string_utils.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

using json = nlohmann::json;

namespace jp_edge_tts {

// ==========================================
// Private Implementation
// ==========================================

class VocabManager::Impl {
public:
    std::unordered_map<std::string, int> token_to_id;
    std::unordered_map<int, std::string> id_to_token;
    int next_id;
    bool is_loaded;

    Impl() : next_id(0), is_loaded(false) {}

    void Clear() {
        token_to_id.clear();
        id_to_token.clear();
        next_id = 0;
        is_loaded = false;
    }

    Status LoadFromJSON(const std::string& json_str) {
        try {
            json j = json::parse(json_str);
            
            // Clear existing vocabulary
            Clear();
            
            // Check if it's an object (token: id mapping)
            if (j.is_object()) {
                for (auto& [token, id] : j.items()) {
                    int token_id;
                    
                    // Handle both string and integer IDs
                    if (id.is_number()) {
                        token_id = id.get<int>();
                    } else if (id.is_string()) {
                        token_id = std::stoi(id.get<std::string>());
                    } else {
                        continue;  // Skip invalid entries
                    }
                    
                    token_to_id[token] = token_id;
                    id_to_token[token_id] = token;
                    next_id = std::max(next_id, token_id + 1);
                }
            }
            // Check if it's an array of tokens
            else if (j.is_array()) {
                int id = 0;
                for (const auto& token : j) {
                    if (token.is_string()) {
                        std::string token_str = token.get<std::string>();
                        token_to_id[token_str] = id;
                        id_to_token[id] = token_str;
                        id++;
                    }
                }
                next_id = id;
            }
            // Check if it has a "vocab" field
            else if (j.contains("vocab")) {
                return LoadFromJSON(j["vocab"].dump());
            }
            else {
                return Status::ERROR_INVALID_INPUT;
            }
            
            is_loaded = true;
            return Status::OK;
            
        } catch (const json::exception& e) {
            std::cerr << "JSON parse error in vocab: " << e.what() << std::endl;
            return Status::ERROR_INVALID_INPUT;
        }
    }

    Status LoadFromText(const std::string& text) {
        // Clear existing vocabulary
        Clear();
        
        std::istringstream stream(text);
        std::string line;
        int id = 0;
        
        while (std::getline(stream, line)) {
            // Skip empty lines and comments
            line = StringUtils::Trim(line);
            if (line.empty() || line[0] == '#') {
                continue;
            }
            
            // Check if line contains ID mapping (token<tab>id or token id)
            size_t separator_pos = line.find('\t');
            if (separator_pos == std::string::npos) {
                separator_pos = line.find(' ');
            }
            
            if (separator_pos != std::string::npos) {
                // Format: token<separator>id
                std::string token = line.substr(0, separator_pos);
                std::string id_str = line.substr(separator_pos + 1);
                
                // Trim whitespace
                token = StringUtils::Trim(token);
                id_str = StringUtils::Trim(id_str);
                
                // Parse ID
                try {
                    int token_id = std::stoi(id_str);
                    token_to_id[token] = token_id;
                    id_to_token[token_id] = token;
                    next_id = std::max(next_id, token_id + 1);
                } catch (...) {
                    // If ID parsing fails, use sequential ID
                    token_to_id[line] = id;
                    id_to_token[id] = line;
                    id++;
                }
            } else {
                // Just the token, assign sequential ID
                token_to_id[line] = id;
                id_to_token[id] = line;
                id++;
            }
        }
        
        if (id > 0) {
            next_id = id;
        }
        
        is_loaded = !token_to_id.empty();
        return is_loaded ? Status::OK : Status::ERROR_INVALID_INPUT;
    }
};

// ==========================================
// Public Interface Implementation
// ==========================================

VocabManager::VocabManager() : pImpl(std::make_unique<Impl>()) {}
VocabManager::~VocabManager() = default;
VocabManager::VocabManager(VocabManager&&) noexcept = default;
VocabManager& VocabManager::operator=(VocabManager&&) noexcept = default;

Status VocabManager::LoadVocabulary(const std::string& vocab_path) {
    // Read vocabulary file
    std::string content = FileUtils::ReadTextFile(vocab_path);
    if (content.empty()) {
        return Status::ERROR_FILE_NOT_FOUND;
    }
    
    // Determine format and load accordingly
    std::string extension = FileUtils::GetExtension(vocab_path);
    
    if (extension == ".json") {
        return pImpl->LoadFromJSON(content);
    } else {
        // Assume text format (one token per line or token<tab>id format)
        return pImpl->LoadFromText(content);
    }
}

Status VocabManager::LoadVocabularyFromJSON(const std::string& json_data) {
    return pImpl->LoadFromJSON(json_data);
}

void VocabManager::AddToken(const std::string& token) {
    // Only add if not already present
    if (pImpl->token_to_id.find(token) == pImpl->token_to_id.end()) {
        int id = pImpl->next_id++;
        pImpl->token_to_id[token] = id;
        pImpl->id_to_token[id] = token;
    }
}

void VocabManager::AddToken(const std::string& token, int id) {
    pImpl->token_to_id[token] = id;
    pImpl->id_to_token[id] = token;
    pImpl->next_id = std::max(pImpl->next_id, id + 1);
}

int VocabManager::GetTokenId(const std::string& token) const {
    auto it = pImpl->token_to_id.find(token);
    if (it != pImpl->token_to_id.end()) {
        return it->second;
    }
    return -1;  // Token not found
}

std::string VocabManager::GetToken(int id) const {
    auto it = pImpl->id_to_token.find(id);
    if (it != pImpl->id_to_token.end()) {
        return it->second;
    }
    return "";  // ID not found
}

bool VocabManager::HasToken(const std::string& token) const {
    return pImpl->token_to_id.find(token) != pImpl->token_to_id.end();
}

bool VocabManager::HasId(int id) const {
    return pImpl->id_to_token.find(id) != pImpl->id_to_token.end();
}

size_t VocabManager::GetVocabularySize() const {
    return pImpl->token_to_id.size();
}

std::vector<std::string> VocabManager::GetAllTokens() const {
    std::vector<std::string> tokens;
    tokens.reserve(pImpl->token_to_id.size());
    
    for (const auto& [token, id] : pImpl->token_to_id) {
        tokens.push_back(token);
    }
    
    // Sort alphabetically for consistency
    std::sort(tokens.begin(), tokens.end());
    return tokens;
}

std::unordered_map<std::string, int> VocabManager::GetTokenToIdMap() const {
    return pImpl->token_to_id;
}

std::unordered_map<int, std::string> VocabManager::GetIdToTokenMap() const {
    return pImpl->id_to_token;
}

void VocabManager::Clear() {
    pImpl->Clear();
}

bool VocabManager::SaveVocabulary(const std::string& output_path) const {
    try {
        std::string extension = FileUtils::GetExtension(output_path);
        
        if (extension == ".json") {
            // Save as JSON
            json j;
            for (const auto& [token, id] : pImpl->token_to_id) {
                j[token] = id;
            }
            
            std::ofstream file(output_path);
            if (!file) {
                return false;
            }
            
            file << j.dump(2);  // Pretty print with 2 spaces
            return file.good();
            
        } else {
            // Save as text (token<tab>id format)
            std::ofstream file(output_path);
            if (!file) {
                return false;
            }
            
            // Create sorted list for consistent output
            std::vector<std::pair<int, std::string>> sorted_vocab;
            for (const auto& [id, token] : pImpl->id_to_token) {
                sorted_vocab.push_back({id, token});
            }
            std::sort(sorted_vocab.begin(), sorted_vocab.end());
            
            // Write tokens in ID order
            for (const auto& [id, token] : sorted_vocab) {
                file << token << "\t" << id << "\n";
            }
            
            return file.good();
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error saving vocabulary: " << e.what() << std::endl;
        return false;
    }
}

bool VocabManager::IsLoaded() const {
    return pImpl->is_loaded;
}

void VocabManager::BuildFromPhonemes(const std::vector<std::string>& phoneme_list) {
    // Clear existing vocabulary
    Clear();
    
    // Add special tokens first
    AddToken("<pad>", 0);
    AddToken("<unk>", 1);
    AddToken("<s>", 2);
    AddToken("</s>", 3);
    
    // Collect unique phonemes
    std::set<std::string> unique_phonemes;
    for (const auto& phoneme_str : phoneme_list) {
        // Split phonemes by space
        auto phonemes = StringUtils::Split(phoneme_str, ' ');
        for (const auto& phoneme : phonemes) {
            if (!phoneme.empty()) {
                unique_phonemes.insert(phoneme);
            }
        }
    }
    
    // Add unique phonemes to vocabulary
    for (const auto& phoneme : unique_phonemes) {
        AddToken(phoneme);
    }
    
    pImpl->is_loaded = true;
}

} // namespace jp_edge_tts