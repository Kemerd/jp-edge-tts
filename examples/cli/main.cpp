/**
 * @file main.cpp
 * @brief Command-line interface for JP Edge TTS
 * D Everett Hinton
 * @date 2025
 *
 * @details A comprehensive CLI application for Japanese text-to-speech synthesis
 * supporting text input, JSON input, and various output options.
 *
 * @copyright MIT License
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#endif

#include "jp_edge_tts/tts_engine.h"
#include "jp_edge_tts/types.h"

// For JSON parsing (using nlohmann/json)
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;
using namespace jp_edge_tts;

/**
 * @class CLIApplication
 * @brief Main command-line application class
 */
class CLIApplication {
public:
    /**
     * @brief Application configuration from command-line arguments
     */
    struct AppConfig {
        std::string input_text;           ///< Direct text input
        std::string input_file;           ///< Input file path (text or JSON)
        std::string output_dir = "output"; ///< Output directory
        std::string output_file;          ///< Specific output file
        std::string voice_id = "";        ///< Voice to use
        float speed = 1.0f;               ///< Speaking speed
        float pitch = 1.0f;               ///< Pitch adjustment
        float volume = 1.0f;              ///< Volume level
        bool interactive = false;         ///< Interactive mode
        bool list_voices = false;         ///< List available voices
        bool verbose = false;             ///< Verbose output
        bool use_json = false;            ///< Input is JSON format
        bool save_phonemes = false;       ///< Save phonemes to file
        bool benchmark = false;           ///< Run benchmark mode
        std::string config_file;          ///< Custom config file
        std::string phonemes;             ///< Pre-computed phonemes
        AudioFormat format = AudioFormat::WAV_PCM16; ///< Output format
    };

    /**
     * @brief Constructor
     */
    CLIApplication() : engine_(nullptr) {
        // Set UTF-8 console on Windows
#ifdef _WIN32
        SetConsoleCP(CP_UTF8);
        SetConsoleOutputCP(CP_UTF8);
        _setmode(_fileno(stdout), _O_U8TEXT);
        _setmode(_fileno(stdin), _O_U8TEXT);
#endif
    }

    /**
     * @brief Run the application
     */
    int Run(int argc, char* argv[]) {
        // Parse command-line arguments
        if (!ParseArguments(argc, argv)) {
            return 1;
        }

        // Handle special modes
        if (config_.list_voices) {
            return ListVoices();
        }

        if (config_.benchmark) {
            return RunBenchmark();
        }

        // Initialize TTS engine
        if (!InitializeEngine()) {
            std::cerr << "Failed to initialize TTS engine" << std::endl;
            return 1;
        }

        // Run appropriate mode
        if (config_.interactive) {
            return RunInteractive();
        } else if (!config_.input_file.empty()) {
            return ProcessFile();
        } else if (!config_.input_text.empty()) {
            return ProcessText(config_.input_text);
        } else {
            PrintUsage();
            return 1;
        }
    }

private:
    AppConfig config_;
    std::unique_ptr<TTSEngine> engine_;

    /**
     * @brief Parse command-line arguments
     */
    bool ParseArguments(int argc, char* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "--help" || arg == "-h") {
                PrintUsage();
                return false;
            } else if (arg == "--version" || arg == "-v") {
                PrintVersion();
                return false;
            } else if (arg == "--list-voices" || arg == "-l") {
                config_.list_voices = true;
            } else if (arg == "--voice" || arg == "-V") {
                if (++i < argc) config_.voice_id = argv[i];
            } else if (arg == "--output" || arg == "-o") {
                if (++i < argc) {
                    std::string output = argv[i];
                    if (fs::is_directory(output) || output.back() == '/' || output.back() == '\\') {
                        config_.output_dir = output;
                    } else {
                        config_.output_file = output;
                    }
                }
            } else if (arg == "--speed" || arg == "-s") {
                if (++i < argc) config_.speed = std::stof(argv[i]);
            } else if (arg == "--pitch" || arg == "-p") {
                if (++i < argc) config_.pitch = std::stof(argv[i]);
            } else if (arg == "--volume") {
                if (++i < argc) config_.volume = std::stof(argv[i]);
            } else if (arg == "--interactive" || arg == "-i") {
                config_.interactive = true;
            } else if (arg == "--file" || arg == "-f") {
                if (++i < argc) config_.input_file = argv[i];
            } else if (arg == "--json" || arg == "-j") {
                config_.use_json = true;
            } else if (arg == "--phonemes") {
                if (++i < argc) config_.phonemes = argv[i];
            } else if (arg == "--save-phonemes") {
                config_.save_phonemes = true;
            } else if (arg == "--format") {
                if (++i < argc) {
                    std::string fmt = argv[i];
                    if (fmt == "wav16") config_.format = AudioFormat::WAV_PCM16;
                    else if (fmt == "wav32") config_.format = AudioFormat::WAV_FLOAT32;
                    else if (fmt == "raw16") config_.format = AudioFormat::RAW_PCM16;
                    else if (fmt == "raw32") config_.format = AudioFormat::RAW_FLOAT32;
                }
            } else if (arg == "--config" || arg == "-c") {
                if (++i < argc) config_.config_file = argv[i];
            } else if (arg == "--verbose") {
                config_.verbose = true;
            } else if (arg == "--benchmark") {
                config_.benchmark = true;
            } else if (arg[0] != '-') {
                // Treat as input text
                config_.input_text = arg;
            }
        }

        return true;
    }

    /**
     * @brief Print usage information
     */
    void PrintUsage() {
        std::cout << R"(
JP Edge TTS - Japanese Text-to-Speech CLI

Usage:
  jp_tts [OPTIONS] [TEXT]
  jp_tts --interactive
  jp_tts --file input.txt --output output.wav
  jp_tts --json --file request.json

Options:
  -h, --help              Show this help message
  -v, --version           Show version information
  -l, --list-voices       List available voices
  -i, --interactive       Interactive mode (type Japanese text)
  -f, --file FILE         Input file (text or JSON)
  -j, --json              Input file is JSON format
  -o, --output PATH       Output directory or file
  -V, --voice ID          Voice to use (e.g., jf_alpha)
  -s, --speed FLOAT       Speaking speed (0.5-2.0, default: 1.0)
  -p, --pitch FLOAT       Pitch adjustment (0.5-2.0, default: 1.0)
  --volume FLOAT          Volume (0.0-1.0, default: 1.0)
  --phonemes IPA          Pre-computed IPA phonemes
  --save-phonemes         Save phonemes to .txt file
  --format FORMAT         Output format (wav16, wav32, raw16, raw32)
  --config FILE           Custom configuration file
  --verbose               Enable verbose output
  --benchmark             Run benchmark mode

Examples:
  # Simple text input
  jp_tts "こんにちは、世界！" --output greeting.wav

  # Interactive mode
  jp_tts --interactive

  # Process text file
  jp_tts --file input.txt --voice jf_alpha --output speeches/

  # JSON input with phonemes
  jp_tts --json --file request.json --output custom.wav

  # With custom settings
  jp_tts "ゆっくり話します" --speed 0.8 --pitch 1.2 --voice jm_kumo

JSON Format:
  {
    "text": "Japanese text here",
    "voice_id": "jf_alpha",
    "speed": 1.0,
    "pitch": 1.0,
    "volume": 1.0,
    "phonemes": "optional IPA phonemes",
    "output": "optional_output.wav"
  }

  Multiple requests in array:
  [
    {"text": "First text", "output": "first.wav"},
    {"text": "Second text", "output": "second.wav"}
  ]
)" << std::endl;
    }

    /**
     * @brief Print version information
     */
    void PrintVersion() {
        std::cout << "JP Edge TTS Version 1.0.0" << std::endl;
        std::cout << "Copyright (c) 2024 JP Edge TTS Project" << std::endl;
        std::cout << "ONNX Runtime Version: " << GetONNXRuntimeVersion() << std::endl;
    }

    /**
     * @brief Get ONNX Runtime version
     */
    std::string GetONNXRuntimeVersion() {
        // This would call the actual ONNX Runtime API
        return "1.16.0";  // Placeholder
    }

    /**
     * @brief Initialize TTS engine
     */
    bool InitializeEngine() {
        // Load configuration
        TTSConfig tts_config;

        if (!config_.config_file.empty()) {
            if (!LoadConfigFile(config_.config_file, tts_config)) {
                std::cerr << "Failed to load config file: " << config_.config_file << std::endl;
                return false;
            }
        }

        tts_config.verbose = config_.verbose;

        // Create and initialize engine
        engine_ = CreateTTSEngine(tts_config);
        if (!engine_) {
            return false;
        }

        if (config_.verbose) {
            std::cout << "Initializing TTS engine..." << std::endl;
        }

        Status status = engine_->Initialize();
        if (status != Status::OK) {
            std::cerr << "Engine initialization failed: " << static_cast<int>(status) << std::endl;
            return false;
        }

        // Load default voices
        LoadDefaultVoices();

        if (config_.verbose) {
            std::cout << "TTS engine initialized successfully" << std::endl;
        }

        return true;
    }

    /**
     * @brief Load configuration from file
     */
    bool LoadConfigFile(const std::string& path, TTSConfig& config) {
        std::ifstream file(path);
        if (!file) return false;

        try {
            json j;
            file >> j;

            if (j.contains("kokoro_model_path"))
                config.kokoro_model_path = j["kokoro_model_path"];
            if (j.contains("dictionary_path"))
                config.dictionary_path = j["dictionary_path"];
            if (j.contains("voices_dir"))
                config.voices_dir = j["voices_dir"];
            if (j.contains("enable_gpu"))
                config.enable_gpu = j["enable_gpu"];
            if (j.contains("enable_cache"))
                config.enable_cache = j["enable_cache"];
            if (j.contains("enable_mecab"))
                config.enable_mecab = j["enable_mecab"];

            return true;
        } catch (const std::exception& e) {
            std::cerr << "Config parse error: " << e.what() << std::endl;
            return false;
        }
    }

    /**
     * @brief Load default voices
     */
    void LoadDefaultVoices() {
        std::vector<std::string> voice_files = {
            "models/voices/jf_alpha.json",
            "models/voices/jf_gongitsune.json",
            "models/voices/jm_kumo.json"
        };

        for (const auto& voice_file : voice_files) {
            if (fs::exists(voice_file)) {
                engine_->LoadVoice(voice_file);
            }
        }
    }

    /**
     * @brief List available voices
     */
    int ListVoices() {
        if (!InitializeEngine()) {
            return 1;
        }

        auto voices = engine_->GetAvailableVoices();

        std::cout << "\nAvailable Voices:\n" << std::endl;
        std::cout << std::left << std::setw(15) << "ID"
                  << std::setw(25) << "Name"
                  << std::setw(10) << "Gender"
                  << "Description" << std::endl;
        std::cout << std::string(70, '-') << std::endl;

        for (const auto& voice : voices) {
            std::cout << std::left << std::setw(15) << voice.id
                      << std::setw(25) << voice.name
                      << std::setw(10) << GenderToString(voice.gender);

            if (voice.description) {
                std::cout << *voice.description;
            }
            std::cout << std::endl;
        }

        return 0;
    }

    /**
     * @brief Convert gender enum to string
     */
    std::string GenderToString(VoiceGender gender) {
        switch (gender) {
            case VoiceGender::MALE: return "Male";
            case VoiceGender::FEMALE: return "Female";
            default: return "Neutral";
        }
    }

    /**
     * @brief Run interactive mode
     */
    int RunInteractive() {
        std::cout << "\nJP Edge TTS Interactive Mode" << std::endl;
        std::cout << "Type Japanese text and press Enter to synthesize." << std::endl;
        std::cout << "Type 'quit' or 'exit' to leave.\n" << std::endl;

        std::string line;
        int file_counter = 1;

        while (true) {
            std::cout << "> ";
            std::getline(std::cin, line);

            if (line == "quit" || line == "exit") {
                break;
            }

            if (line.empty()) {
                continue;
            }

            // Generate output filename
            std::string output_file = GenerateOutputFilename(file_counter++);

            // Process the text
            if (ProcessTextToFile(line, output_file) == 0) {
                std::cout << "✓ Saved to: " << output_file << std::endl;
            }
        }

        std::cout << "\nGoodbye!" << std::endl;
        return 0;
    }

    /**
     * @brief Process input file
     */
    int ProcessFile() {
        if (config_.use_json) {
            return ProcessJSONFile();
        } else {
            return ProcessTextFile();
        }
    }

    /**
     * @brief Process text file
     */
    int ProcessTextFile() {
        std::ifstream file(config_.input_file);
        if (!file) {
            std::cerr << "Cannot open file: " << config_.input_file << std::endl;
            return 1;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string text = buffer.str();

        return ProcessText(text);
    }

    /**
     * @brief Process JSON file
     */
    int ProcessJSONFile() {
        std::ifstream file(config_.input_file);
        if (!file) {
            std::cerr << "Cannot open file: " << config_.input_file << std::endl;
            return 1;
        }

        try {
            json j;
            file >> j;

            if (j.is_array()) {
                // Process multiple requests
                int success_count = 0;
                for (const auto& item : j) {
                    if (ProcessJSONRequest(item) == 0) {
                        success_count++;
                    }
                }
                std::cout << "Processed " << success_count << "/" << j.size() << " requests" << std::endl;
            } else {
                // Single request
                return ProcessJSONRequest(j);
            }
        } catch (const std::exception& e) {
            std::cerr << "JSON parse error: " << e.what() << std::endl;
            return 1;
        }

        return 0;
    }

    /**
     * @brief Process single JSON request
     */
    int ProcessJSONRequest(const json& request) {
        // Extract parameters
        TTSRequest tts_request;

        tts_request.text = request.value("text", "");
        tts_request.voice_id = request.value("voice_id", config_.voice_id);
        tts_request.speed = request.value("speed", config_.speed);
        tts_request.pitch = request.value("pitch", config_.pitch);
        tts_request.volume = request.value("volume", config_.volume);

        if (request.contains("phonemes")) {
            tts_request.ipa_phonemes = request["phonemes"];
        }

        if (request.contains("vocabulary_id")) {
            tts_request.vocabulary_id = request["vocabulary_id"];
        }

        // Determine output file
        std::string output_file;
        if (request.contains("output")) {
            output_file = request["output"];
        } else if (!config_.output_file.empty()) {
            output_file = config_.output_file;
        } else {
            output_file = GenerateOutputFilename();
        }

        // Synthesize
        if (config_.verbose) {
            std::cout << "Processing: \"" << tts_request.text.substr(0, 50)
                      << "...\" -> " << output_file << std::endl;
        }

        auto result = engine_->Synthesize(tts_request);

        if (result.IsSuccess()) {
            SaveResult(result, output_file);
            return 0;
        } else {
            std::cerr << "Synthesis failed: " << result.error_message << std::endl;
            return 1;
        }
    }

    /**
     * @brief Process text input
     */
    int ProcessText(const std::string& text) {
        std::string output_file = config_.output_file.empty() ?
                                 GenerateOutputFilename() : config_.output_file;

        return ProcessTextToFile(text, output_file);
    }

    /**
     * @brief Process text and save to file
     */
    int ProcessTextToFile(const std::string& text, const std::string& output_file) {
        // Create TTS request
        TTSRequest request;
        request.text = text;
        request.voice_id = config_.voice_id;
        request.speed = config_.speed;
        request.pitch = config_.pitch;
        request.volume = config_.volume;
        request.format = config_.format;

        if (!config_.phonemes.empty()) {
            request.ipa_phonemes = config_.phonemes;
        }

        // Synthesize
        auto start = std::chrono::high_resolution_clock::now();

        TTSResult result = engine_->Synthesize(request);

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        if (!result.IsSuccess()) {
            std::cerr << "Synthesis failed: " << result.error_message << std::endl;
            return 1;
        }

        // Save result
        SaveResult(result, output_file);

        if (config_.verbose) {
            PrintStats(result, duration.count());
        }

        return 0;
    }

    /**
     * @brief Save synthesis result
     */
    void SaveResult(const TTSResult& result, const std::string& output_file) {
        // Create output directory if needed
        fs::path output_path(output_file);
        if (output_path.has_parent_path()) {
            fs::create_directories(output_path.parent_path());
        }

        // Save audio
        engine_->SaveAudioToFile(result.audio, output_file, config_.format);

        // Save phonemes if requested
        if (config_.save_phonemes) {
            std::string phoneme_file = output_path.stem().string() + "_phonemes.txt";
            SavePhonemes(result.phonemes, phoneme_file);
        }
    }

    /**
     * @brief Save phonemes to file
     */
    void SavePhonemes(const std::vector<PhonemeInfo>& phonemes, const std::string& filename) {
        std::ofstream file(filename);
        for (const auto& p : phonemes) {
            file << p.phoneme << " ";
        }
        file << std::endl;
    }

    /**
     * @brief Generate output filename
     */
    std::string GenerateOutputFilename(int counter = -1) {
        // Create output directory
        fs::create_directories(config_.output_dir);

        // Generate timestamp-based filename
        if (counter < 0) {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << config_.output_dir << "/tts_"
               << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S")
               << ".wav";
            return ss.str();
        } else {
            std::stringstream ss;
            ss << config_.output_dir << "/tts_" << std::setfill('0')
               << std::setw(4) << counter << ".wav";
            return ss.str();
        }
    }

    /**
     * @brief Print synthesis statistics
     */
    void PrintStats(const TTSResult& result, int64_t total_ms) {
        std::cout << "\nSynthesis Statistics:" << std::endl;
        std::cout << "  Total time: " << total_ms << " ms" << std::endl;
        std::cout << "  Text length: " << result.stats.text_length << " characters" << std::endl;
        std::cout << "  Phonemes: " << result.stats.phoneme_count << std::endl;
        std::cout << "  Tokens: " << result.stats.token_count << std::endl;
        std::cout << "  Audio duration: " << result.audio.duration.count() << " ms" << std::endl;
        std::cout << "  Samples: " << result.audio.samples.size() << std::endl;
        std::cout << "  Cache hit: " << (result.stats.cache_hit ? "Yes" : "No") << std::endl;
    }

    /**
     * @brief Run benchmark mode
     */
    int RunBenchmark() {
        if (!InitializeEngine()) {
            return 1;
        }

        std::vector<std::string> test_texts = {
            "こんにちは",
            "今日はいい天気ですね",
            "日本の技術は世界一です",
            "明日は雨が降るでしょう",
            "ありがとうございました"
        };

        std::cout << "\nRunning benchmark..." << std::endl;

        int64_t total_time = 0;
        int success_count = 0;

        for (size_t i = 0; i < test_texts.size(); ++i) {
            auto start = std::chrono::high_resolution_clock::now();

            auto result = engine_->SynthesizeSimple(test_texts[i]);

            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

            if (result.IsSuccess()) {
                success_count++;
                total_time += duration.count();
                std::cout << "  Test " << (i + 1) << ": " << duration.count() << " ms" << std::endl;
            } else {
                std::cout << "  Test " << (i + 1) << ": FAILED" << std::endl;
            }
        }

        std::cout << "\nBenchmark Results:" << std::endl;
        std::cout << "  Success rate: " << success_count << "/" << test_texts.size() << std::endl;
        std::cout << "  Average time: " << (total_time / success_count) << " ms" << std::endl;
        std::cout << "  Total time: " << total_time << " ms" << std::endl;

        return 0;
    }
};

/**
 * @brief Main entry point
 */
int main(int argc, char* argv[]) {
    try {
        CLIApplication app;
        return app.Run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}