/**
 * @file jp_edge_tts_c_api.cpp
 * @brief Implementation of C API wrapper for Japanese TTS engine
 * @author D Everett Hinton
 * @date 2025
 *
 * @copyright MIT License
 */

#include "jp_edge_tts/jp_edge_tts_c_api.h"
#include "jp_edge_tts/core/tts_engine.h"
#include <iostream>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <mutex>

// Internal structure to hold engine instances
static std::unordered_map<jp_tts_engine_t, std::unique_ptr<jp_edge_tts::TTSEngine>> g_engines;
static std::unordered_map<jp_tts_result_t, std::unique_ptr<jp_edge_tts::TTSResult>> g_results;
static std::mutex g_engines_mutex;
static std::mutex g_results_mutex;
static jp_tts_engine_t g_next_engine_id = 1;
static jp_tts_result_t g_next_result_id = 1;

// Thread-local error buffer
thread_local char g_error_buffer[256] = {0};

// Helper functions
namespace {
    void SetError(const char* message) {
        std::strncpy(g_error_buffer, message, sizeof(g_error_buffer) - 1);
        g_error_buffer[sizeof(g_error_buffer) - 1] = '\0';
    }

    jp_edge_tts::AudioFormat ConvertAudioFormat(jp_tts_audio_format_t format) {
        switch (format) {
            case JP_TTS_FORMAT_WAV_PCM16:
                return jp_edge_tts::AudioFormat::WAV_PCM16;
            case JP_TTS_FORMAT_WAV_PCM24:
                return jp_edge_tts::AudioFormat::WAV_PCM24;
            case JP_TTS_FORMAT_WAV_FLOAT32:
                return jp_edge_tts::AudioFormat::WAV_FLOAT32;
            default:
                return jp_edge_tts::AudioFormat::WAV_PCM16;
        }
    }

    jp_tts_status_t ConvertStatus(jp_edge_tts::Status status) {
        switch (status) {
            case jp_edge_tts::Status::OK:
                return JP_TTS_OK;
            case jp_edge_tts::Status::ERROR_INVALID_INPUT:
                return JP_TTS_ERROR_INVALID_INPUT;
            case jp_edge_tts::Status::ERROR_FILE_NOT_FOUND:
                return JP_TTS_ERROR_FILE_NOT_FOUND;
            case jp_edge_tts::Status::ERROR_NOT_INITIALIZED:
                return JP_TTS_ERROR_NOT_INITIALIZED;
            case jp_edge_tts::Status::ERROR_INITIALIZATION_FAILED:
                return JP_TTS_ERROR_INITIALIZATION_FAILED;
            case jp_edge_tts::Status::ERROR_SYNTHESIS_FAILED:
                return JP_TTS_ERROR_SYNTHESIS_FAILED;
            case jp_edge_tts::Status::ERROR_VOICE_NOT_FOUND:
                return JP_TTS_ERROR_VOICE_NOT_FOUND;
            case jp_edge_tts::Status::ERROR_CANCELLED:
                return JP_TTS_ERROR_CANCELLED;
            case jp_edge_tts::Status::ERROR_FILE_WRITE_FAILED:
                return JP_TTS_ERROR_FILE_WRITE_FAILED;
            default:
                return JP_TTS_ERROR_UNKNOWN;
        }
    }
}

// ==========================================
// Engine Management
// ==========================================

jp_tts_engine_t jp_tts_create_engine(const jp_tts_config_t* config) {
    try {
        jp_edge_tts::TTSConfig cpp_config;

        if (config) {
            if (config->model_path) {
                cpp_config.model_path = config->model_path;
            }
            if (config->voice_dir) {
                cpp_config.voice_dir = config->voice_dir;
            }
            if (config->mecab_dic_path) {
                cpp_config.mecab_dic_path = config->mecab_dic_path;
            }
            if (config->phoneme_dict_path) {
                cpp_config.phoneme_dict_path = config->phoneme_dict_path;
            }
            if (config->vocab_path) {
                cpp_config.vocab_path = config->vocab_path;
            }

            cpp_config.enable_gpu = config->enable_gpu;
            cpp_config.enable_cache = config->enable_cache;
            cpp_config.sample_rate = config->sample_rate > 0 ? config->sample_rate : 22050;
            cpp_config.num_threads = config->num_threads > 0 ? config->num_threads : 4;
            cpp_config.cache_size_mb = config->cache_size_mb > 0 ? config->cache_size_mb : 100;
        }

        auto engine = jp_edge_tts::CreateTTSEngine(cpp_config);

        std::lock_guard<std::mutex> lock(g_engines_mutex);
        jp_tts_engine_t handle = g_next_engine_id++;
        g_engines[handle] = std::move(engine);

        return handle;

    } catch (const std::exception& e) {
        SetError(e.what());
        return 0;
    }
}

jp_tts_status_t jp_tts_initialize(jp_tts_engine_t engine) {
    std::lock_guard<std::mutex> lock(g_engines_mutex);
    auto it = g_engines.find(engine);
    if (it == g_engines.end()) {
        SetError("Invalid engine handle");
        return JP_TTS_ERROR_INVALID_INPUT;
    }

    try {
        auto status = it->second->Initialize();
        return ConvertStatus(status);
    } catch (const std::exception& e) {
        SetError(e.what());
        return JP_TTS_ERROR_INITIALIZATION_FAILED;
    }
}

void jp_tts_destroy_engine(jp_tts_engine_t engine) {
    std::lock_guard<std::mutex> lock(g_engines_mutex);
    g_engines.erase(engine);
}

int jp_tts_is_initialized(jp_tts_engine_t engine) {
    std::lock_guard<std::mutex> lock(g_engines_mutex);
    auto it = g_engines.find(engine);
    if (it == g_engines.end()) {
        return 0;
    }
    return it->second->IsInitialized() ? 1 : 0;
}

// ==========================================
// Synthesis Operations
// ==========================================

jp_tts_result_t jp_tts_synthesize_simple(jp_tts_engine_t engine,
                                          const char* text,
                                          const char* voice_id) {
    if (!text) {
        SetError("Text cannot be null");
        return 0;
    }

    std::lock_guard<std::mutex> engine_lock(g_engines_mutex);
    auto engine_it = g_engines.find(engine);
    if (engine_it == g_engines.end()) {
        SetError("Invalid engine handle");
        return 0;
    }

    try {
        std::string voice_str = voice_id ? voice_id : "";
        auto result = engine_it->second->SynthesizeSimple(text, voice_str);

        // Store result
        auto result_ptr = std::make_unique<jp_edge_tts::TTSResult>(std::move(result));

        std::lock_guard<std::mutex> result_lock(g_results_mutex);
        jp_tts_result_t handle = g_next_result_id++;
        g_results[handle] = std::move(result_ptr);

        return handle;

    } catch (const std::exception& e) {
        SetError(e.what());
        return 0;
    }
}

jp_tts_result_t jp_tts_synthesize(jp_tts_engine_t engine,
                                  const jp_tts_request_t* request) {
    if (!request || !request->text) {
        SetError("Request and text cannot be null");
        return 0;
    }

    std::lock_guard<std::mutex> engine_lock(g_engines_mutex);
    auto engine_it = g_engines.find(engine);
    if (engine_it == g_engines.end()) {
        SetError("Invalid engine handle");
        return 0;
    }

    try {
        jp_edge_tts::TTSRequest cpp_request;
        cpp_request.text = request->text;

        if (request->voice_id) {
            cpp_request.voice_id = request->voice_id;
        }
        if (request->phonemes) {
            cpp_request.phonemes = request->phonemes;
        }

        cpp_request.speed = request->speed > 0 ? request->speed : 1.0f;
        cpp_request.pitch = request->pitch > 0 ? request->pitch : 1.0f;
        cpp_request.volume = request->volume > 0 ? request->volume : 1.0f;

        auto result = engine_it->second->Synthesize(cpp_request);

        // Store result
        auto result_ptr = std::make_unique<jp_edge_tts::TTSResult>(std::move(result));

        std::lock_guard<std::mutex> result_lock(g_results_mutex);
        jp_tts_result_t handle = g_next_result_id++;
        g_results[handle] = std::move(result_ptr);

        return handle;

    } catch (const std::exception& e) {
        SetError(e.what());
        return 0;
    }
}

// ==========================================
// Result Management
// ==========================================

jp_tts_status_t jp_tts_result_get_status(jp_tts_result_t result) {
    std::lock_guard<std::mutex> lock(g_results_mutex);
    auto it = g_results.find(result);
    if (it == g_results.end()) {
        return JP_TTS_ERROR_INVALID_INPUT;
    }
    return ConvertStatus(it->second->status);
}

const char* jp_tts_result_get_error(jp_tts_result_t result) {
    std::lock_guard<std::mutex> lock(g_results_mutex);
    auto it = g_results.find(result);
    if (it == g_results.end()) {
        return "Invalid result handle";
    }
    return it->second->error_message.c_str();
}

size_t jp_tts_result_get_audio_size(jp_tts_result_t result) {
    std::lock_guard<std::mutex> lock(g_results_mutex);
    auto it = g_results.find(result);
    if (it == g_results.end()) {
        return 0;
    }
    return it->second->audio.data.size() * sizeof(float);
}

const float* jp_tts_result_get_audio_data(jp_tts_result_t result) {
    std::lock_guard<std::mutex> lock(g_results_mutex);
    auto it = g_results.find(result);
    if (it == g_results.end()) {
        return nullptr;
    }
    return it->second->audio.data.data();
}

int jp_tts_result_get_sample_rate(jp_tts_result_t result) {
    std::lock_guard<std::mutex> lock(g_results_mutex);
    auto it = g_results.find(result);
    if (it == g_results.end()) {
        return 0;
    }
    return it->second->audio.sample_rate;
}

int jp_tts_result_get_channels(jp_tts_result_t result) {
    std::lock_guard<std::mutex> lock(g_results_mutex);
    auto it = g_results.find(result);
    if (it == g_results.end()) {
        return 0;
    }
    return it->second->audio.channels;
}

jp_tts_status_t jp_tts_result_save_to_file(jp_tts_result_t result,
                                           const char* filepath,
                                           jp_tts_audio_format_t format) {
    if (!filepath) {
        SetError("Filepath cannot be null");
        return JP_TTS_ERROR_INVALID_INPUT;
    }

    std::lock_guard<std::mutex> lock(g_results_mutex);
    auto it = g_results.find(result);
    if (it == g_results.end()) {
        SetError("Invalid result handle");
        return JP_TTS_ERROR_INVALID_INPUT;
    }

    // We need to get the engine to save the file
    // For simplicity, we'll create a temporary engine instance
    try {
        jp_edge_tts::TTSConfig config;
        auto temp_engine = jp_edge_tts::CreateTTSEngine(config);

        auto status = temp_engine->SaveAudioToFile(
            it->second->audio,
            filepath,
            ConvertAudioFormat(format)
        );

        return ConvertStatus(status);

    } catch (const std::exception& e) {
        SetError(e.what());
        return JP_TTS_ERROR_FILE_WRITE_FAILED;
    }
}

void jp_tts_result_free(jp_tts_result_t result) {
    std::lock_guard<std::mutex> lock(g_results_mutex);
    g_results.erase(result);
}

// ==========================================
// Voice Management
// ==========================================

jp_tts_status_t jp_tts_load_voice(jp_tts_engine_t engine, const char* voice_path) {
    if (!voice_path) {
        SetError("Voice path cannot be null");
        return JP_TTS_ERROR_INVALID_INPUT;
    }

    std::lock_guard<std::mutex> lock(g_engines_mutex);
    auto it = g_engines.find(engine);
    if (it == g_engines.end()) {
        SetError("Invalid engine handle");
        return JP_TTS_ERROR_INVALID_INPUT;
    }

    try {
        auto status = it->second->LoadVoice(voice_path);
        return ConvertStatus(status);
    } catch (const std::exception& e) {
        SetError(e.what());
        return JP_TTS_ERROR_FILE_NOT_FOUND;
    }
}

size_t jp_tts_get_voice_count(jp_tts_engine_t engine) {
    std::lock_guard<std::mutex> lock(g_engines_mutex);
    auto it = g_engines.find(engine);
    if (it == g_engines.end()) {
        return 0;
    }

    try {
        auto voices = it->second->GetAvailableVoices();
        return voices.size();
    } catch (const std::exception&) {
        return 0;
    }
}

int jp_tts_get_voice_ids(jp_tts_engine_t engine, char** voice_ids, size_t max_voices) {
    if (!voice_ids) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(g_engines_mutex);
    auto it = g_engines.find(engine);
    if (it == g_engines.end()) {
        return 0;
    }

    try {
        auto voices = it->second->GetAvailableVoices();
        size_t count = std::min(voices.size(), max_voices);

        for (size_t i = 0; i < count; i++) {
            // Allocate memory for voice ID string
            size_t len = voices[i].id.length();
            voice_ids[i] = new char[len + 1];
            std::strcpy(voice_ids[i], voices[i].id.c_str());
        }

        return static_cast<int>(count);

    } catch (const std::exception&) {
        return 0;
    }
}

void jp_tts_free_voice_ids(char** voice_ids, size_t count) {
    if (!voice_ids) {
        return;
    }

    for (size_t i = 0; i < count; i++) {
        delete[] voice_ids[i];
    }
}

// ==========================================
// Utility Functions
// ==========================================

const char* jp_tts_get_last_error() {
    return g_error_buffer;
}

const char* jp_tts_get_version() {
    return jp_edge_tts::GetVersion().c_str();
}

int jp_tts_is_gpu_available() {
    return jp_edge_tts::IsGPUAvailable() ? 1 : 0;
}

void jp_tts_clear_cache(jp_tts_engine_t engine) {
    std::lock_guard<std::mutex> lock(g_engines_mutex);
    auto it = g_engines.find(engine);
    if (it != g_engines.end()) {
        it->second->ClearCache();
    }
}

size_t jp_tts_get_queue_size(jp_tts_engine_t engine) {
    std::lock_guard<std::mutex> lock(g_engines_mutex);
    auto it = g_engines.find(engine);
    if (it == g_engines.end()) {
        return 0;
    }
    return it->second->GetQueueSize();
}

size_t jp_tts_get_active_synthesis_count(jp_tts_engine_t engine) {
    std::lock_guard<std::mutex> lock(g_engines_mutex);
    auto it = g_engines.find(engine);
    if (it == g_engines.end()) {
        return 0;
    }
    return it->second->GetActiveSynthesisCount();
}

// ==========================================
// Advanced Features
// ==========================================

char* jp_tts_text_to_phonemes(jp_tts_engine_t engine, const char* text) {
    if (!text) {
        SetError("Text cannot be null");
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(g_engines_mutex);
    auto it = g_engines.find(engine);
    if (it == g_engines.end()) {
        SetError("Invalid engine handle");
        return nullptr;
    }

    try {
        auto phonemes = it->second->TextToPhonemes(text);

        // Convert PhonemeInfo vector to string
        std::string result;
        for (size_t i = 0; i < phonemes.size(); i++) {
            if (i > 0) result += " ";
            result += phonemes[i].phoneme;
        }

        // Allocate C string
        char* c_result = new char[result.length() + 1];
        std::strcpy(c_result, result.c_str());
        return c_result;

    } catch (const std::exception& e) {
        SetError(e.what());
        return nullptr;
    }
}

void jp_tts_free_phonemes(char* phonemes) {
    delete[] phonemes;
}

jp_tts_status_t jp_tts_warmup(jp_tts_engine_t engine) {
    std::lock_guard<std::mutex> lock(g_engines_mutex);
    auto it = g_engines.find(engine);
    if (it == g_engines.end()) {
        SetError("Invalid engine handle");
        return JP_TTS_ERROR_INVALID_INPUT;
    }

    try {
        auto status = it->second->Warmup();
        return ConvertStatus(status);
    } catch (const std::exception& e) {
        SetError(e.what());
        return JP_TTS_ERROR_SYNTHESIS_FAILED;
    }
}

void jp_tts_shutdown(jp_tts_engine_t engine) {
    std::lock_guard<std::mutex> lock(g_engines_mutex);
    auto it = g_engines.find(engine);
    if (it != g_engines.end()) {
        it->second->Shutdown();
    }
}

// ==========================================
// Cleanup on library unload
// ==========================================

#ifdef _WIN32
#include <windows.h>

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved) {
    switch (dwReason) {
        case DLL_PROCESS_DETACH:
            // Clean up global resources
            g_engines.clear();
            g_results.clear();
            break;
    }
    return TRUE;
}

#else

__attribute__((destructor))
void cleanup_jp_tts() {
    g_engines.clear();
    g_results.clear();
}

#endif