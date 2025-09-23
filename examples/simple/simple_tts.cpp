/**
 * @file simple_tts.cpp
 * @brief Simple example demonstrating basic JP Edge TTS usage
 * @author D Everett Hinton
 * @date 2025
 *
 * @copyright MIT License
 */

#include "jp_edge_tts/core/tts_engine.h"
#include <iostream>
#include <string>

using namespace jp_edge_tts;

/**
 * @brief Print available voices
 */
void printAvailableVoices(const TTSEngine& engine) {
    std::cout << "\nAvailable voices:" << std::endl;
    auto voices = engine.GetAvailableVoices();

    if (voices.empty()) {
        std::cout << "  No voices loaded!" << std::endl;
        return;
    }

    for (const auto& voice : voices) {
        std::cout << "  " << voice.id << " - " << voice.name;
        std::cout << " (" << (voice.gender == VoiceGender::MALE ? "Male" :
                              voice.gender == VoiceGender::FEMALE ? "Female" : "Neutral") << ")";
        if (voice.description) {
            std::cout << " - " << *voice.description;
        }
        std::cout << std::endl;
    }
}

/**
 * @brief Demonstrate basic text-to-speech synthesis
 */
void demonstrateBasicTTS(TTSEngine& engine) {
    std::cout << "\n=== Basic TTS Demo ===" << std::endl;

    // Simple synthesis with default voice
    std::string text = "こんにちは、JP Edge TTS へようこそ！";
    std::cout << "Synthesizing: \"" << text << "\"" << std::endl;

    auto result = engine.SynthesizeSimple(text);

    if (result.IsSuccess()) {
        std::cout << "✓ Synthesis successful!" << std::endl;
        std::cout << "  Audio duration: " << result.audio.duration.count() << " ms" << std::endl;
        std::cout << "  Audio samples: " << result.audio.samples.size() << std::endl;
        std::cout << "  Processing time: " << result.stats.total_time.count() << " ms" << std::endl;
        std::cout << "  Phonemes: " << result.phonemes.size() << std::endl;

        // Save to file
        auto save_status = engine.SaveAudioToFile(result.audio, "simple_output.wav");
        if (save_status == Status::OK) {
            std::cout << "  Audio saved to: simple_output.wav" << std::endl;
        }
    } else {
        std::cout << "✗ Synthesis failed: " << result.error_message << std::endl;
    }
}

/**
 * @brief Demonstrate synthesis with custom parameters
 */
void demonstrateCustomSynthesis(TTSEngine& engine, const std::string& voice_id) {
    std::cout << "\n=== Custom Synthesis Demo ===" << std::endl;

    TTSRequest request;
    request.text = "速度とピッチを調整したテストです。";
    request.voice_id = voice_id;
    request.speed = 1.2f;  // 20% faster
    request.pitch = 1.1f;  // 10% higher pitch
    request.volume = 0.8f; // 80% volume

    std::cout << "Synthesizing with custom parameters:" << std::endl;
    std::cout << "  Text: \"" << request.text << "\"" << std::endl;
    std::cout << "  Voice: " << voice_id << std::endl;
    std::cout << "  Speed: " << request.speed << "x" << std::endl;
    std::cout << "  Pitch: " << request.pitch << "x" << std::endl;
    std::cout << "  Volume: " << request.volume << std::endl;

    auto result = engine.Synthesize(request);

    if (result.IsSuccess()) {
        std::cout << "✓ Custom synthesis successful!" << std::endl;
        std::cout << "  Processing breakdown:" << std::endl;
        std::cout << "    Phonemization: " << result.stats.phonemization_time.count() << " ms" << std::endl;
        std::cout << "    Tokenization: " << result.stats.tokenization_time.count() << " ms" << std::endl;
        std::cout << "    Inference: " << result.stats.inference_time.count() << " ms" << std::endl;
        std::cout << "    Audio processing: " << result.stats.audio_processing_time.count() << " ms" << std::endl;

        // Save to file
        engine.SaveAudioToFile(result.audio, "custom_output.wav");
        std::cout << "  Audio saved to: custom_output.wav" << std::endl;
    } else {
        std::cout << "✗ Custom synthesis failed: " << result.error_message << std::endl;
    }
}

/**
 * @brief Demonstrate text analysis capabilities
 */
void demonstrateTextAnalysis(TTSEngine& engine) {
    std::cout << "\n=== Text Analysis Demo ===" << std::endl;

    std::string text = "今日は良い天気です。";
    std::cout << "Analyzing text: \"" << text << "\"" << std::endl;

    // Normalize text
    auto normalized = engine.NormalizeText(text);
    std::cout << "Normalized: \"" << normalized << "\"" << std::endl;

    // Get phonemes
    auto phonemes = engine.TextToPhonemes(text);
    std::cout << "Phonemes (" << phonemes.size() << "):" << std::endl;
    for (size_t i = 0; i < phonemes.size() && i < 10; ++i) {
        std::cout << "  " << phonemes[i].phoneme;
        if (i < phonemes.size() - 1) std::cout << " ";
    }
    if (phonemes.size() > 10) {
        std::cout << " ... (+" << (phonemes.size() - 10) << " more)";
    }
    std::cout << std::endl;

    // Segment text
    auto segments = engine.SegmentText(text);
    std::cout << "Segments (" << segments.size() << "):" << std::endl;
    for (const auto& segment : segments) {
        std::cout << "  \"" << segment << "\"" << std::endl;
    }
}

/**
 * @brief Demonstrate asynchronous synthesis
 */
void demonstrateAsyncSynthesis(TTSEngine& engine, const std::string& voice_id) {
    std::cout << "\n=== Async Synthesis Demo ===" << std::endl;

    std::vector<std::string> texts = {
        "非同期合成のテスト一番目",
        "非同期合成のテスト二番目",
        "非同期合成のテスト三番目"
    };

    std::cout << "Submitting " << texts.size() << " async requests..." << std::endl;

    // Submit all requests
    std::vector<std::future<TTSResult>> futures;
    for (size_t i = 0; i < texts.size(); ++i) {
        TTSRequest request;
        request.text = texts[i];
        request.voice_id = voice_id;

        futures.push_back(engine.SynthesizeAsync(request));
        std::cout << "  Submitted request " << (i + 1) << std::endl;
    }

    // Collect results
    std::cout << "Waiting for results..." << std::endl;
    for (size_t i = 0; i < futures.size(); ++i) {
        auto result = futures[i].get();
        if (result.IsSuccess()) {
            std::cout << "  ✓ Request " << (i + 1) << " completed in "
                      << result.stats.total_time.count() << " ms" << std::endl;

            // Save each result
            std::string filename = "async_output_" + std::to_string(i + 1) + ".wav";
            engine.SaveAudioToFile(result.audio, filename);
        } else {
            std::cout << "  ✗ Request " << (i + 1) << " failed: "
                      << result.error_message << std::endl;
        }
    }
}

int main(int argc, char* argv[]) {
    std::cout << "JP Edge TTS Simple Example" << std::endl;
    std::cout << "=========================" << std::endl;

    // Parse command line arguments
    std::string voice_id = "jf_alpha";  // Default voice
    if (argc > 1) {
        voice_id = argv[1];
    }

    // Create and configure engine
    TTSConfig config;
    config.enable_cache = true;
    config.verbose = true;  // Enable verbose logging for demo

    auto engine = CreateTTSEngine(config);

    // Initialize engine
    std::cout << "Initializing TTS engine..." << std::endl;
    auto status = engine->Initialize();

    if (status != Status::OK) {
        std::cerr << "Failed to initialize TTS engine!" << std::endl;
        std::cerr << "Make sure models and data files are in the correct locations." << std::endl;
        return 1;
    }

    std::cout << "✓ Engine initialized successfully!" << std::endl;

    // Show available voices
    printAvailableVoices(*engine);

    // Check if requested voice exists
    auto voice = engine->GetVoice(voice_id);
    if (!voice) {
        std::cout << "Warning: Voice '" << voice_id << "' not found, using default." << std::endl;
        auto voices = engine->GetAvailableVoices();
        if (!voices.empty()) {
            voice_id = voices[0].id;
            std::cout << "Using voice: " << voice_id << std::endl;
        }
    }

    // Run demonstrations
    demonstrateBasicTTS(*engine);
    demonstrateCustomSynthesis(*engine, voice_id);
    demonstrateTextAnalysis(*engine);
    demonstrateAsyncSynthesis(*engine, voice_id);

    // Show final statistics
    std::cout << "\n=== Final Statistics ===" << std::endl;
    auto perf_stats = engine->GetPerformanceStats();
    std::cout << "Total requests: " << perf_stats.total_requests << std::endl;
    std::cout << "Successful: " << perf_stats.successful_requests << std::endl;
    std::cout << "Failed: " << perf_stats.failed_requests << std::endl;

    auto cache_stats = engine->GetCacheStats();
    std::cout << "Cache hits: " << cache_stats.hit_count << std::endl;
    std::cout << "Cache misses: " << cache_stats.miss_count << std::endl;

    std::cout << "\nDemo completed! Check the generated .wav files." << std::endl;

    return 0;
}