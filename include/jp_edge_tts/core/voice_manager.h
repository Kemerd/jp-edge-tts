/**
 * @file voice_manager.h
 * @brief Voice loading and management for TTS
 * D Everett Hinton
 * @date 2025
 *
 * @copyright MIT License
 */

#ifndef JP_EDGE_TTS_VOICE_MANAGER_H
#define JP_EDGE_TTS_VOICE_MANAGER_H

#include "jp_edge_tts/types.h"
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <optional>

namespace jp_edge_tts {

/**
 * @class VoiceManager
 * @brief Manages voice data and configurations for TTS
 *
 * @details Handles loading voice JSON files, managing voice
 * style vectors, and providing voice selection capabilities.
 */
class VoiceManager {
public:
    /**
     * @brief Constructor
     */
    VoiceManager();

    /**
     * @brief Destructor
     */
    ~VoiceManager();

    // Disable copy, enable move
    VoiceManager(const VoiceManager&) = delete;
    VoiceManager& operator=(const VoiceManager&) = delete;
    VoiceManager(VoiceManager&&) noexcept;
    VoiceManager& operator=(VoiceManager&&) noexcept;

    /**
     * @brief Load voice from JSON file
     *
     * @param voice_path Path to voice JSON file
     * @return Status code
     */
    Status LoadVoice(const std::string& voice_path);

    /**
     * @brief Load voice from memory
     *
     * @param voice_id Unique voice identifier
     * @param json_data JSON data as string
     * @return Status code
     */
    Status LoadVoiceFromJSON(const std::string& voice_id,
                             const std::string& json_data);

    /**
     * @brief Load all voices from directory
     *
     * @param directory Path to voices directory
     * @return Number of voices loaded
     */
    int LoadVoicesFromDirectory(const std::string& directory);

    /**
     * @brief Get voice by ID
     *
     * @param voice_id Voice identifier
     * @return Voice data if found
     */
    std::optional<Voice> GetVoice(const std::string& voice_id) const;

    /**
     * @brief Get all loaded voices
     * @return Vector of all voices
     */
    std::vector<Voice> GetAllVoices() const;

    /**
     * @brief Get list of voice IDs
     * @return Vector of voice identifiers
     */
    std::vector<std::string> GetVoiceIds() const;

    /**
     * @brief Check if voice exists
     *
     * @param voice_id Voice identifier
     * @return true if voice is loaded
     */
    bool HasVoice(const std::string& voice_id) const;

    /**
     * @brief Set default voice
     *
     * @param voice_id Voice identifier
     * @return true if successful
     */
    bool SetDefaultVoice(const std::string& voice_id);

    /**
     * @brief Get default voice ID
     * @return Default voice identifier
     */
    std::string GetDefaultVoiceId() const;

    /**
     * @brief Unload voice
     *
     * @param voice_id Voice identifier
     * @return true if voice was unloaded
     */
    bool UnloadVoice(const std::string& voice_id);

    /**
     * @brief Clear all voices
     */
    void ClearVoices();

    /**
     * @brief Get number of loaded voices
     * @return Voice count
     */
    size_t GetVoiceCount() const;

    /**
     * @brief Get memory usage of loaded voices
     * @return Memory usage in bytes
     */
    size_t GetMemoryUsage() const;

    /**
     * @brief Export voice to JSON file
     *
     * @param voice_id Voice identifier
     * @param output_path Output file path
     * @return true if successful
     */
    bool ExportVoice(const std::string& voice_id,
                     const std::string& output_path) const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace jp_edge_tts

#endif // JP_EDGE_TTS_VOICE_MANAGER_H