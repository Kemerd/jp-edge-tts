/**
 * @file tts_engine.cpp
 * @brief Implementation of the main TTS Engine class
 * @author D Everett Hinton
 * @date 2025
 *
 * @copyright MIT License
 */

#include "jp_edge_tts/core/tts_engine.h"
#include "jp_edge_tts/core/session_manager.h"
#include "jp_edge_tts/core/voice_manager.h"
#include "jp_edge_tts/core/cache_manager.h"
#include "jp_edge_tts/phonemizer/japanese_phonemizer.h"
#include "jp_edge_tts/tokenizer/ipa_tokenizer.h"
#include "jp_edge_tts/audio/audio_processor.h"
#include "jp_edge_tts/utils/thread_pool.h"
#include "jp_edge_tts/utils/string_utils.h"

#include <iostream>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace jp_edge_tts {

// ==========================================
// Private Implementation Class
// ==========================================

class TTSEngine::Impl {
public:
    TTSConfig config;
    std::unique_ptr<SessionManager> session_manager;
    std::unique_ptr<VoiceManager> voice_manager;
    std::unique_ptr<CacheManager> cache_manager;
    std::unique_ptr<JapanesePhonemizer> phonemizer;
    std::unique_ptr<IPATokenizer> tokenizer;
    std::unique_ptr<AudioProcessor> audio_processor;
    std::unique_ptr<ThreadPool> thread_pool;

    // State tracking
    std::atomic<bool> initialized{false};
    std::atomic<size_t> active_synthesis_count{0};
    std::atomic<size_t> total_requests{0};
    std::atomic<size_t> successful_requests{0};
    std::atomic<size_t> failed_requests{0};

    // Request queue
    struct QueuedRequest {
        std::string id;
        TTSRequest request;
        std::promise<TTSResult> promise;
        AudioCallback callback;
        std::chrono::steady_clock::time_point submitted;
    };

    std::queue<QueuedRequest> request_queue;
    std::mutex queue_mutex;
    std::condition_variable queue_cv;
    std::thread queue_processor;
    std::atomic<bool> stop_processing{false};

    // Callbacks
    ProgressCallback progress_callback;
    ErrorCallback error_callback;

    // Performance tracking
    std::vector<std::chrono::milliseconds> latency_history;
    std::mutex stats_mutex;

    // Last error message
    std::string last_error;

    /**
     * @brief Constructor
     */
    Impl(const TTSConfig& cfg) : config(cfg) {
        // Initialize components
        session_manager = std::make_unique<SessionManager>();
        voice_manager = std::make_unique<VoiceManager>();
        cache_manager = std::make_unique<CacheManager>(config.max_cache_size_mb * 1024 * 1024);

        // Create thread pool for parallel processing
        int num_threads = config.max_concurrent_requests > 0 ?
                         config.max_concurrent_requests : std::thread::hardware_concurrency();
        thread_pool = std::make_unique<ThreadPool>(num_threads);
    }

    /**
     * @brief Destructor
     */
    ~Impl() {
        // Stop queue processing
        stop_processing = true;
        queue_cv.notify_all();
        if (queue_processor.joinable()) {
            queue_processor.join();
        }
    }

    /**
     * @brief Initialize all components
     */
    Status Initialize() {
        try {
            // Initialize ONNX session for Kokoro model
            if (!session_manager->LoadModel(config.kokoro_model_path)) {
                last_error = "Failed to load Kokoro model from: " + config.kokoro_model_path;
                return Status::ERROR_MODEL_NOT_LOADED;
            }

            // Initialize phonemizer
            JapanesePhonemizer::Config phonemizer_config;
            phonemizer_config.dictionary_path = config.dictionary_path;
            phonemizer_config.onnx_model_path = config.phonemizer_model_path;
            phonemizer_config.use_mecab = config.enable_mecab;
            phonemizer_config.enable_cache = config.enable_cache;

            phonemizer = std::make_unique<JapanesePhonemizer>(phonemizer_config);
            auto status = phonemizer->Initialize();
            if (status != Status::OK) {
                last_error = "Failed to initialize phonemizer";
                return status;
            }

            // Initialize tokenizer
            tokenizer = std::make_unique<IPATokenizer>();
            if (!tokenizer->LoadVocabulary(config.tokenizer_vocab_path)) {
                last_error = "Failed to load tokenizer vocabulary from: " + config.tokenizer_vocab_path;
                return Status::ERROR_FILE_NOT_FOUND;
            }

            // Initialize audio processor
            audio_processor = std::make_unique<AudioProcessor>(config.target_sample_rate);

            // Load default voices
            LoadVoicesFromDirectory(config.voices_dir);

            // Start queue processor thread
            queue_processor = std::thread(&Impl::ProcessQueue, this);

            initialized = true;
            return Status::OK;

        } catch (const std::exception& e) {
            last_error = std::string("Initialization error: ") + e.what();
            return Status::ERROR_INVALID_INPUT;
        }
    }

    /**
     * @brief Load voices from directory
     */
    void LoadVoicesFromDirectory(const std::string& dir) {
        namespace fs = std::filesystem;

        if (!fs::exists(dir)) {
            return;
        }

        for (const auto& entry : fs::directory_iterator(dir)) {
            if (entry.path().extension() == ".json") {
                voice_manager->LoadVoice(entry.path().string());
            }
        }
    }

    /**
     * @brief Process synthesis request
     */
    TTSResult ProcessSynthesis(const TTSRequest& request) {
        TTSResult result;
        auto start_time = std::chrono::high_resolution_clock::now();

        try {
            // Update statistics
            result.stats.text_length = request.text.length();

            // Check cache first
            std::string cache_key = GenerateCacheKey(request);
            if (request.use_cache) {
                auto cached = cache_manager->Get(cache_key);
                if (cached) {
                    result = *cached;
                    result.stats.cache_hit = true;
                    return result;
                }
            }

            // Step 1: Text normalization
            std::string normalized_text = request.normalize_text ?
                                         phonemizer->NormalizeText(request.text) : request.text;

            // Step 2: Phonemization
            auto phoneme_start = std::chrono::high_resolution_clock::now();
            std::string phonemes;

            if (request.ipa_phonemes.has_value()) {
                // Use provided phonemes
                phonemes = *request.ipa_phonemes;
            } else {
                // Generate phonemes
                phonemes = phonemizer->Phonemize(normalized_text);
            }

            auto phoneme_end = std::chrono::high_resolution_clock::now();
            result.stats.phonemization_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                phoneme_end - phoneme_start);

            // Parse phonemes for result
            result.phonemes = ParsePhonemes(phonemes);
            result.stats.phoneme_count = result.phonemes.size();

            // Step 3: Tokenization
            auto token_start = std::chrono::high_resolution_clock::now();
            std::vector<int> tokens = tokenizer->PhonemesToTokens(phonemes);

            auto token_end = std::chrono::high_resolution_clock::now();
            result.stats.tokenization_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                token_end - token_start);
            result.stats.token_count = tokens.size();

            // Step 4: Get voice
            auto voice = voice_manager->GetVoice(request.voice_id);
            if (!voice) {
                result.status = Status::ERROR_INVALID_INPUT;
                result.error_message = "Voice not found: " + request.voice_id;
                return result;
            }

            // Step 5: ONNX inference
            auto inference_start = std::chrono::high_resolution_clock::now();

            auto audio_samples = session_manager->RunInference(
                tokens,
                voice->style_vector,
                request.speed * voice->default_speed,
                request.pitch * voice->default_pitch
            );

            auto inference_end = std::chrono::high_resolution_clock::now();
            result.stats.inference_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                inference_end - inference_start);

            // Step 6: Audio post-processing
            auto audio_start = std::chrono::high_resolution_clock::now();

            result.audio.samples = audio_processor->ProcessAudio(
                audio_samples,
                request.volume,
                config.normalize_audio
            );

            result.audio.sample_rate = config.target_sample_rate;
            result.audio.channels = 1;
            result.audio.duration = std::chrono::milliseconds(
                static_cast<int64_t>(result.audio.samples.size() * 1000 / config.target_sample_rate)
            );

            auto audio_end = std::chrono::high_resolution_clock::now();
            result.stats.audio_processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                audio_end - audio_start);

            result.stats.audio_samples = result.audio.samples.size();

            // Calculate total time
            auto end_time = std::chrono::high_resolution_clock::now();
            result.stats.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - start_time);

            // Update cache
            if (request.use_cache) {
                cache_manager->Put(cache_key, result);
            }

            // Update statistics
            successful_requests++;
            result.status = Status::OK;

        } catch (const std::exception& e) {
            result.status = Status::ERROR_INFERENCE_FAILED;
            result.error_message = e.what();
            failed_requests++;
        }

        // Track latency
        {
            std::lock_guard<std::mutex> lock(stats_mutex);
            latency_history.push_back(result.stats.total_time);
            if (latency_history.size() > 1000) {
                latency_history.erase(latency_history.begin());
            }
        }

        return result;
    }

    /**
     * @brief Process request queue
     */
    void ProcessQueue() {
        while (!stop_processing) {
            std::unique_lock<std::mutex> lock(queue_mutex);

            // Wait for requests or stop signal
            queue_cv.wait(lock, [this] {
                return !request_queue.empty() || stop_processing;
            });

            if (stop_processing) break;

            // Get next request
            if (!request_queue.empty()) {
                auto queued = std::move(request_queue.front());
                request_queue.pop();
                lock.unlock();

                // Process request
                active_synthesis_count++;
                auto result = ProcessSynthesis(queued.request);
                active_synthesis_count--;

                // Fulfill promise
                queued.promise.set_value(result);

                // Call callback if provided
                if (queued.callback && result.IsSuccess()) {
                    queued.callback(result.audio);
                }
            }
        }
    }

    /**
     * @brief Generate cache key for request
     */
    std::string GenerateCacheKey(const TTSRequest& request) {
        std::stringstream ss;
        ss << request.text << "|"
           << request.voice_id << "|"
           << request.speed << "|"
           << request.pitch << "|"
           << request.volume;

        // Simple hash (in production, use proper hash like SHA256)
        std::hash<std::string> hasher;
        return std::to_string(hasher(ss.str()));
    }

    /**
     * @brief Parse phoneme string into structured format
     */
    std::vector<PhonemeInfo> ParsePhonemes(const std::string& phonemes) {
        std::vector<PhonemeInfo> result;
        std::istringstream stream(phonemes);
        std::string phoneme;
        int position = 0;

        while (stream >> phoneme) {
            PhonemeInfo info;
            info.phoneme = phoneme;
            info.position = position++;
            result.push_back(info);
        }

        return result;
    }
};

// ==========================================
// Public Interface Implementation
// ==========================================

TTSEngine::TTSEngine(const TTSConfig& config)
    : pImpl(std::make_unique<Impl>(config)) {
}

TTSEngine::~TTSEngine() = default;
TTSEngine::TTSEngine(TTSEngine&&) noexcept = default;
TTSEngine& TTSEngine::operator=(TTSEngine&&) noexcept = default;

Status TTSEngine::Initialize() {
    return pImpl->Initialize();
}

bool TTSEngine::IsInitialized() const {
    return pImpl->initialized;
}

Status TTSEngine::UpdateConfig(const TTSConfig& config) {
    pImpl->config = config;
    // Some settings might require re-initialization
    return Status::OK;
}

const TTSConfig& TTSEngine::GetConfig() const {
    return pImpl->config;
}

TTSResult TTSEngine::SynthesizeSimple(const std::string& text, const std::string& voice_id) {
    TTSRequest request;
    request.text = text;
    request.voice_id = voice_id.empty() ? pImpl->voice_manager->GetDefaultVoiceId() : voice_id;
    return Synthesize(request);
}

TTSResult TTSEngine::Synthesize(const TTSRequest& request) {
    if (!pImpl->initialized) {
        TTSResult result;
        result.status = Status::ERROR_NOT_INITIALIZED;
        result.error_message = "Engine not initialized";
        return result;
    }

    pImpl->total_requests++;
    return pImpl->ProcessSynthesis(request);
}

std::future<TTSResult> TTSEngine::SynthesizeAsync(const TTSRequest& request) {
    auto promise = std::make_shared<std::promise<TTSResult>>();
    auto future = promise->get_future();

    if (!pImpl->initialized) {
        TTSResult result;
        result.status = Status::ERROR_NOT_INITIALIZED;
        result.error_message = "Engine not initialized";
        promise->set_value(result);
        return future;
    }

    // Submit to thread pool
    pImpl->thread_pool->enqueue([this, request, promise]() {
        try {
            pImpl->total_requests++;
            auto result = pImpl->ProcessSynthesis(request);
            promise->set_value(result);
        } catch (...) {
            TTSResult error_result;
            error_result.status = Status::ERROR_UNKNOWN;
            error_result.error_message = "Unknown error in async synthesis";
            promise->set_value(error_result);
        }
    });

    return future;
}

Status TTSEngine::LoadVoice(const std::string& voice_path) {
    return pImpl->voice_manager->LoadVoice(voice_path);
}

std::vector<Voice> TTSEngine::GetAvailableVoices() const {
    return pImpl->voice_manager->GetAllVoices();
}

void TTSEngine::ClearCache() {
    pImpl->cache_manager->Clear();
}

Status TTSEngine::SaveAudioToFile(const AudioData& audio, const std::string& filepath, AudioFormat format) {
    return pImpl->audio_processor->SaveToFile(audio, filepath, format);
}

size_t TTSEngine::GetQueueSize() const {
    std::lock_guard<std::mutex> lock(pImpl->queue_mutex);
    return pImpl->request_queue.size();
}

size_t TTSEngine::GetActiveSynthesisCount() const {
    return pImpl->active_synthesis_count;
}

// ==========================================
// Factory Functions
// ==========================================

std::unique_ptr<TTSEngine> CreateTTSEngine() {
    return std::make_unique<TTSEngine>();
}

std::unique_ptr<TTSEngine> CreateTTSEngine(const TTSConfig& config) {
    return std::make_unique<TTSEngine>(config);
}

std::string GetVersion() {
    return "1.0.0";
}

bool IsGPUAvailable() {
    // Check CUDA availability
#ifdef USE_CUDA
    return true;  // Would check actual CUDA device availability
#else
    return false;
#endif
}

} // namespace jp_edge_tts