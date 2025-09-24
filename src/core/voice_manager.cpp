/**
 * @file voice_manager.cpp
 * @brief Implementation of voice loading and management
 * @author D Everett Hinton
 * @date 2025
 *
 * @copyright MIT License
 */

#include "jp_edge_tts/core/voice_manager.h"
#include "jp_edge_tts/utils/file_utils.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <mutex>
#include <unordered_map>

using json = nlohmann::json;

namespace jp_edge_tts {

// ==========================================
// Private Implementation
// ==========================================

class VoiceManager::Impl {
public:
    std::unordered_map<std::string, Voice> voices;
    std::string default_voice_id;
    mutable std::mutex mutex;

    Status LoadVoice(const std::string& voice_path) {
        try {
            // Read JSON file
            std::string json_str = FileUtils::ReadTextFile(voice_path);
            if (json_str.empty()) {
                return Status::ERROR_FILE_NOT_FOUND;
            }

            // Parse JSON
            json j = json::parse(json_str);

            // Extract voice ID from filename or JSON
            std::string voice_id;
            if (j.contains("id")) {
                voice_id = j["id"].get<std::string>();
            } else {
                // Use filename without extension as ID
                voice_id = FileUtils::GetStem(voice_path);
            }

            return LoadVoiceFromJSON(voice_id, json_str);

        } catch (const json::exception& e) {
            std::cerr << "JSON parse error: " << e.what() << std::endl;
            return Status::ERROR_INVALID_INPUT;
        } catch (const std::exception& e) {
            std::cerr << "Error loading voice: " << e.what() << std::endl;
            return Status::ERROR_FILE_NOT_FOUND;
        }
    }

    Status LoadVoiceFromJSON(const std::string& voice_id, const std::string& json_data) {
        try {
            json j = json::parse(json_data);
            Voice voice;

            // Set ID
            voice.id = voice_id;

            // Parse basic info
            if (j.contains("name")) {
                voice.name = j["name"].get<std::string>();
            } else {
                voice.name = voice_id;
            }

            if (j.contains("language")) {
                voice.language = j["language"].get<std::string>();
            }

            if (j.contains("gender")) {
                std::string gender = j["gender"].get<std::string>();
                if (gender == "male" || gender == "MALE") {
                    voice.gender = VoiceGender::MALE;
                } else if (gender == "female" || gender == "FEMALE") {
                    voice.gender = VoiceGender::FEMALE;
                } else {
                    voice.gender = VoiceGender::NEUTRAL;
                }
            }

            // Parse style vector - this is critical for Kokoro
            if (j.contains("style") || j.contains("style_vector")) {
                auto style_key = j.contains("style") ? "style" : "style_vector";

                if (j[style_key].is_array()) {
                    voice.style_vector = j[style_key].get<std::vector<float>>();
                } else if (j[style_key].is_string()) {
                    // Base64 encoded style vector
                    std::string encoded = j[style_key].get<std::string>();
                    voice.style_vector = DecodeBase64FloatVector(encoded);
                }
            } else {
                // Generate default style vector if not provided
                voice.style_vector.resize(128, 0.0f);  // Kokoro uses 128-dim
            }

            // Parse optional parameters
            if (j.contains("default_speed")) {
                voice.default_speed = j["default_speed"].get<float>();
            }

            if (j.contains("default_pitch")) {
                voice.default_pitch = j["default_pitch"].get<float>();
            }

            if (j.contains("description")) {
                voice.description = j["description"].get<std::string>();
            }

            if (j.contains("preview_url")) {
                voice.preview_url = j["preview_url"].get<std::string>();
            }

            // Add to voices map
            {
                std::lock_guard<std::mutex> lock(mutex);
                voices[voice_id] = voice;

                // Set as default if first voice
                if (voices.size() == 1 || default_voice_id.empty()) {
                    default_voice_id = voice_id;
                }
            }

            return Status::OK;

        } catch (const json::exception& e) {
            std::cerr << "JSON error: " << e.what() << std::endl;
            return Status::ERROR_INVALID_INPUT;
        }
    }

    std::vector<float> DecodeBase64FloatVector(const std::string& encoded) {
        // Simple base64 decoding for float vector
        // TODO: Implement proper base64 decoding
        // For now, return default vector
        std::vector<float> result(128, 0.0f);

        // Generate slightly random values for testing
        for (size_t i = 0; i < result.size(); i++) {
            result[i] = (i % 10) / 10.0f - 0.5f;
        }

        return result;
    }
};

// ==========================================
// Public Interface Implementation
// ==========================================

VoiceManager::VoiceManager() : pImpl(std::make_unique<Impl>()) {}
VoiceManager::~VoiceManager() = default;
VoiceManager::VoiceManager(VoiceManager&&) noexcept = default;
VoiceManager& VoiceManager::operator=(VoiceManager&&) noexcept = default;

Status VoiceManager::LoadVoice(const std::string& voice_path) {
    return pImpl->LoadVoice(voice_path);
}

Status VoiceManager::LoadVoiceFromJSON(const std::string& voice_id, const std::string& json_data) {
    return pImpl->LoadVoiceFromJSON(voice_id, json_data);
}

int VoiceManager::LoadVoicesFromDirectory(const std::string& directory) {
    if (!FileUtils::IsDirectory(directory)) {
        return 0;
    }

    auto files = FileUtils::ListFiles(directory, ".json");
    int loaded = 0;

    for (const auto& file : files) {
        if (LoadVoice(file) == Status::OK) {
            loaded++;
        }
    }

    return loaded;
}

std::optional<Voice> VoiceManager::GetVoice(const std::string& voice_id) const {
    std::lock_guard<std::mutex> lock(pImpl->mutex);

    auto it = pImpl->voices.find(voice_id);
    if (it != pImpl->voices.end()) {
        return it->second;
    }

    return std::nullopt;
}

std::vector<Voice> VoiceManager::GetAllVoices() const {
    std::lock_guard<std::mutex> lock(pImpl->mutex);

    std::vector<Voice> result;
    for (const auto& [id, voice] : pImpl->voices) {
        result.push_back(voice);
    }

    return result;
}

std::vector<std::string> VoiceManager::GetVoiceIds() const {
    std::lock_guard<std::mutex> lock(pImpl->mutex);

    std::vector<std::string> result;
    for (const auto& [id, voice] : pImpl->voices) {
        result.push_back(id);
    }

    return result;
}

bool VoiceManager::HasVoice(const std::string& voice_id) const {
    std::lock_guard<std::mutex> lock(pImpl->mutex);
    return pImpl->voices.find(voice_id) != pImpl->voices.end();
}

bool VoiceManager::SetDefaultVoice(const std::string& voice_id) {
    std::lock_guard<std::mutex> lock(pImpl->mutex);

    if (pImpl->voices.find(voice_id) != pImpl->voices.end()) {
        pImpl->default_voice_id = voice_id;
        return true;
    }

    return false;
}

std::string VoiceManager::GetDefaultVoiceId() const {
    std::lock_guard<std::mutex> lock(pImpl->mutex);
    return pImpl->default_voice_id;
}

bool VoiceManager::UnloadVoice(const std::string& voice_id) {
    std::lock_guard<std::mutex> lock(pImpl->mutex);

    auto it = pImpl->voices.find(voice_id);
    if (it != pImpl->voices.end()) {
        pImpl->voices.erase(it);

        // Reset default if needed
        if (pImpl->default_voice_id == voice_id) {
            if (!pImpl->voices.empty()) {
                pImpl->default_voice_id = pImpl->voices.begin()->first;
            } else {
                pImpl->default_voice_id.clear();
            }
        }

        return true;
    }

    return false;
}

void VoiceManager::ClearVoices() {
    std::lock_guard<std::mutex> lock(pImpl->mutex);
    pImpl->voices.clear();
    pImpl->default_voice_id.clear();
}

size_t VoiceManager::GetVoiceCount() const {
    std::lock_guard<std::mutex> lock(pImpl->mutex);
    return pImpl->voices.size();
}

size_t VoiceManager::GetMemoryUsage() const {
    std::lock_guard<std::mutex> lock(pImpl->mutex);

    size_t total = 0;
    for (const auto& [id, voice] : pImpl->voices) {
        total += sizeof(Voice);
        total += voice.id.capacity();
        total += voice.name.capacity();
        total += voice.language.capacity();
        total += voice.style_vector.capacity() * sizeof(float);
        if (voice.description) {
            total += voice.description->capacity();
        }
        if (voice.preview_url) {
            total += voice.preview_url->capacity();
        }
    }

    return total;
}

bool VoiceManager::ExportVoice(const std::string& voice_id, const std::string& output_path) const {
    std::lock_guard<std::mutex> lock(pImpl->mutex);

    auto it = pImpl->voices.find(voice_id);
    if (it == pImpl->voices.end()) {
        return false;
    }

    const Voice& voice = it->second;

    try {
        json j;
        j["id"] = voice.id;
        j["name"] = voice.name;
        j["language"] = voice.language;

        // Convert gender enum to string
        switch (voice.gender) {
            case VoiceGender::MALE:
                j["gender"] = "male";
                break;
            case VoiceGender::FEMALE:
                j["gender"] = "female";
                break;
            default:
                j["gender"] = "neutral";
        }

        j["style_vector"] = voice.style_vector;
        j["default_speed"] = voice.default_speed;
        j["default_pitch"] = voice.default_pitch;

        if (voice.description) {
            j["description"] = *voice.description;
        }

        if (voice.preview_url) {
            j["preview_url"] = *voice.preview_url;
        }

        // Write to file
        std::ofstream file(output_path);
        if (!file) {
            return false;
        }

        file << j.dump(2);  // Pretty print with 2 spaces
        return file.good();

    } catch (const std::exception& e) {
        std::cerr << "Error exporting voice: " << e.what() << std::endl;
        return false;
    }
}

} // namespace jp_edge_tts