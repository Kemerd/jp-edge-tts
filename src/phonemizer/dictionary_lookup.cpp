/**
 * @file dictionary_lookup.cpp
 * @brief Implementation of dictionary-based phoneme lookup
 * @author D Everett Hinton
 * @date 2025
 *
 * @copyright MIT License
 */

#include "jp_edge_tts/phonemizer/dictionary_lookup.h"
#include "jp_edge_tts/utils/file_utils.h"
#include "jp_edge_tts/utils/string_utils.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <regex>

using json = nlohmann::json;

namespace jp_edge_tts {

// ==========================================
// Private Implementation
// ==========================================

class DictionaryLookup::Impl {
public:
    // Main dictionary: word/phrase -> phonemes
    std::unordered_map<std::string, std::string> dictionary;
    
    // Reading-specific dictionary for Japanese
    std::unordered_map<std::string, std::vector<ReadingEntry>> reading_dict;
    
    // Statistics
    size_t total_entries = 0;
    size_t lookup_hits = 0;
    size_t lookup_misses = 0;

    Status LoadFromJSON(const std::string& json_str) {
        try {
            json j = json::parse(json_str);
            
            // Clear existing dictionary
            dictionary.clear();
            reading_dict.clear();
            
            // Check if it has a dictionary field
            json dict_data = j;
            if (j.contains("dictionary")) {
                dict_data = j["dictionary"];
            } else if (j.contains("entries")) {
                dict_data = j["entries"];
            }
            
            // Load simple word->phoneme mappings
            if (dict_data.is_object()) {
                for (auto& [word, value] : dict_data.items()) {
                    if (value.is_string()) {
                        // Simple mapping: word -> phonemes
                        dictionary[word] = value.get<std::string>();
                    } 
                    else if (value.is_object()) {
                        // Complex entry with readings
                        LoadComplexEntry(word, value);
                    }
                    else if (value.is_array()) {
                        // Multiple readings for the same word
                        LoadMultipleReadings(word, value);
                    }
                }
            }
            else if (dict_data.is_array()) {
                // Array format: [{"word": "...", "phonemes": "..."}, ...]
                for (const auto& entry : dict_data) {
                    if (entry.contains("word") && entry.contains("phonemes")) {
                        std::string word = entry["word"].get<std::string>();
                        std::string phonemes = entry["phonemes"].get<std::string>();
                        dictionary[word] = phonemes;
                        
                        // Also check for reading field
                        if (entry.contains("reading")) {
                            ReadingEntry reading_entry;
                            reading_entry.reading = entry["reading"].get<std::string>();
                            reading_entry.phonemes = phonemes;
                            
                            if (entry.contains("pos")) {
                                reading_entry.pos = entry["pos"].get<std::string>();
                            }
                            
                            reading_dict[word].push_back(reading_entry);
                        }
                    }
                }
            }
            
            total_entries = dictionary.size();
            return Status::OK;
            
        } catch (const json::exception& e) {
            std::cerr << "JSON parse error in dictionary: " << e.what() << std::endl;
            return Status::ERROR_INVALID_INPUT;
        }
    }

    void LoadComplexEntry(const std::string& word, const json& entry) {
        // Handle entries with multiple fields
        if (entry.contains("phonemes")) {
            dictionary[word] = entry["phonemes"].get<std::string>();
        }
        
        // Handle reading-specific entries
        if (entry.contains("readings") && entry["readings"].is_array()) {
            for (const auto& reading_entry : entry["readings"]) {
                ReadingEntry re;
                
                if (reading_entry.contains("reading")) {
                    re.reading = reading_entry["reading"].get<std::string>();
                }
                
                if (reading_entry.contains("phonemes")) {
                    re.phonemes = reading_entry["phonemes"].get<std::string>();
                }
                
                if (reading_entry.contains("pos")) {
                    re.pos = reading_entry["pos"].get<std::string>();
                }
                
                if (reading_entry.contains("context")) {
                    re.context = reading_entry["context"].get<std::string>();
                }
                
                reading_dict[word].push_back(re);
            }
        }
    }

    void LoadMultipleReadings(const std::string& word, const json& readings) {
        // First entry is the default
        bool first = true;
        
        for (const auto& reading : readings) {
            if (reading.is_string()) {
                // Simple phoneme string
                if (first) {
                    dictionary[word] = reading.get<std::string>();
                    first = false;
                }
                
                ReadingEntry re;
                re.phonemes = reading.get<std::string>();
                reading_dict[word].push_back(re);
            }
            else if (reading.is_object()) {
                // Complex reading entry
                LoadComplexEntry(word, reading);
                first = false;
            }
        }
    }

    std::string NormalizeWord(const std::string& word) {
        // Normalize for lookup (lowercase, trim)
        std::string normalized = StringUtils::ToLower(word);
        normalized = StringUtils::Trim(normalized);
        return normalized;
    }

    bool MatchesContext(const ReadingEntry& entry, const std::string& context) {
        // Simple context matching
        if (entry.context.empty() || context.empty()) {
            return true;  // No context requirement
        }
        
        // Check if context contains the required pattern
        return context.find(entry.context) != std::string::npos;
    }
};

// ==========================================
// Public Interface Implementation
// ==========================================

DictionaryLookup::DictionaryLookup() : pImpl(std::make_unique<Impl>()) {}
DictionaryLookup::~DictionaryLookup() = default;
DictionaryLookup::DictionaryLookup(DictionaryLookup&&) noexcept = default;
DictionaryLookup& DictionaryLookup::operator=(DictionaryLookup&&) noexcept = default;

Status DictionaryLookup::LoadDictionary(const std::string& dict_path) {
    // Read dictionary file
    std::string content = FileUtils::ReadTextFile(dict_path);
    if (content.empty()) {
        return Status::ERROR_FILE_NOT_FOUND;
    }
    
    return pImpl->LoadFromJSON(content);
}

Status DictionaryLookup::LoadDictionaryFromJSON(const std::string& json_data) {
    return pImpl->LoadFromJSON(json_data);
}

std::optional<std::string> DictionaryLookup::Lookup(const std::string& word) const {
    // Normalize the word for lookup
    std::string normalized = pImpl->NormalizeWord(word);
    
    // Look up in dictionary
    auto it = pImpl->dictionary.find(normalized);
    if (it != pImpl->dictionary.end()) {
        pImpl->lookup_hits++;
        return it->second;
    }
    
    // Try without normalization (for case-sensitive entries)
    it = pImpl->dictionary.find(word);
    if (it != pImpl->dictionary.end()) {
        pImpl->lookup_hits++;
        return it->second;
    }
    
    pImpl->lookup_misses++;
    return std::nullopt;
}

std::optional<std::string> DictionaryLookup::LookupWithReading(
    const std::string& word,
    const std::string& reading,
    const std::string& pos) const {
    
    // First try exact lookup
    auto exact_result = Lookup(word);
    
    // Check reading-specific entries
    auto it = pImpl->reading_dict.find(word);
    if (it != pImpl->reading_dict.end()) {
        const auto& readings = it->second;
        
        // Find best matching reading
        for (const auto& entry : readings) {
            // Match by reading
            if (!reading.empty() && entry.reading == reading) {
                // Also check POS if provided
                if (pos.empty() || entry.pos.empty() || entry.pos == pos) {
                    pImpl->lookup_hits++;
                    return entry.phonemes;
                }
            }
            // Match by POS only
            else if (!pos.empty() && entry.pos == pos && reading.empty()) {
                pImpl->lookup_hits++;
                return entry.phonemes;
            }
        }
        
        // No specific match, return first reading if available
        if (!readings.empty() && !readings[0].phonemes.empty()) {
            pImpl->lookup_hits++;
            return readings[0].phonemes;
        }
    }
    
    // Return exact lookup result if available
    if (exact_result.has_value()) {
        return exact_result;
    }
    
    pImpl->lookup_misses++;
    return std::nullopt;
}

std::vector<std::string> DictionaryLookup::BatchLookup(
    const std::vector<std::string>& words) const {
    
    std::vector<std::string> results;
    results.reserve(words.size());
    
    for (const auto& word : words) {
        auto result = Lookup(word);
        results.push_back(result.value_or(""));
    }
    
    return results;
}

bool DictionaryLookup::Contains(const std::string& word) const {
    std::string normalized = pImpl->NormalizeWord(word);
    return pImpl->dictionary.find(normalized) != pImpl->dictionary.end() ||
           pImpl->dictionary.find(word) != pImpl->dictionary.end();
}

size_t DictionaryLookup::GetDictionarySize() const {
    return pImpl->total_entries;
}

void DictionaryLookup::AddEntry(const std::string& word, const std::string& phonemes) {
    pImpl->dictionary[word] = phonemes;
    pImpl->total_entries = pImpl->dictionary.size();
}

void DictionaryLookup::AddReadingEntry(
    const std::string& word,
    const std::string& reading,
    const std::string& phonemes,
    const std::string& pos,
    const std::string& context) {
    
    ReadingEntry entry;
    entry.reading = reading;
    entry.phonemes = phonemes;
    entry.pos = pos;
    entry.context = context;
    
    pImpl->reading_dict[word].push_back(entry);
    
    // Also add as default if no default exists
    if (pImpl->dictionary.find(word) == pImpl->dictionary.end()) {
        pImpl->dictionary[word] = phonemes;
        pImpl->total_entries = pImpl->dictionary.size();
    }
}

void DictionaryLookup::Clear() {
    pImpl->dictionary.clear();
    pImpl->reading_dict.clear();
    pImpl->total_entries = 0;
    pImpl->lookup_hits = 0;
    pImpl->lookup_misses = 0;
}

DictionaryStats DictionaryLookup::GetStats() const {
    DictionaryStats stats;
    stats.total_entries = pImpl->total_entries;
    stats.unique_words = pImpl->dictionary.size();
    stats.words_with_readings = pImpl->reading_dict.size();
    stats.lookup_hits = pImpl->lookup_hits;
    stats.lookup_misses = pImpl->lookup_misses;
    
    // Calculate hit rate
    size_t total_lookups = stats.lookup_hits + stats.lookup_misses;
    stats.hit_rate = (total_lookups > 0) ? 
                     static_cast<float>(stats.lookup_hits) / total_lookups : 0.0f;
    
    return stats;
}

void DictionaryLookup::ResetStats() {
    pImpl->lookup_hits = 0;
    pImpl->lookup_misses = 0;
}

bool DictionaryLookup::SaveDictionary(const std::string& output_path) const {
    try {
        json j;
        
        // Save main dictionary
        json dict_obj;
        for (const auto& [word, phonemes] : pImpl->dictionary) {
            dict_obj[word] = phonemes;
        }
        j["dictionary"] = dict_obj;
        
        // Save reading entries if any
        if (!pImpl->reading_dict.empty()) {
            json readings_obj;
            
            for (const auto& [word, readings] : pImpl->reading_dict) {
                json word_readings = json::array();
                
                for (const auto& entry : readings) {
                    json reading_entry;
                    if (!entry.reading.empty()) reading_entry["reading"] = entry.reading;
                    reading_entry["phonemes"] = entry.phonemes;
                    if (!entry.pos.empty()) reading_entry["pos"] = entry.pos;
                    if (!entry.context.empty()) reading_entry["context"] = entry.context;
                    
                    word_readings.push_back(reading_entry);
                }
                
                readings_obj[word] = word_readings;
            }
            
            j["readings"] = readings_obj;
        }
        
        // Add metadata
        j["metadata"] = {
            {"total_entries", pImpl->total_entries},
            {"unique_words", pImpl->dictionary.size()},
            {"words_with_readings", pImpl->reading_dict.size()}
        };
        
        // Write to file
        std::ofstream file(output_path);
        if (!file) {
            return false;
        }
        
        file << j.dump(2);  // Pretty print with 2 spaces
        return file.good();
        
    } catch (const std::exception& e) {
        std::cerr << "Error saving dictionary: " << e.what() << std::endl;
        return false;
    }
}

} // namespace jp_edge_tts