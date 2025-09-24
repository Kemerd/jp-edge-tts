/**
 * @file phonemizer_onnx.cpp
 * @brief Implementation of ONNX-based phonemizer for G2P conversion
 * @author D Everett Hinton
 * @date 2025
 *
 * @copyright MIT License
 */

#include "jp_edge_tts/phonemizer/phonemizer_onnx.h"
#include <onnxruntime_cxx_api.h>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <codecvt>
#include <locale>

namespace jp_edge_tts {

// ==========================================
// Private Implementation
// ==========================================

class PhonemizerONNX::Impl {
public:
    std::unique_ptr<Ort::Session> session;
    std::unique_ptr<Ort::Env> env;
    Ort::MemoryInfo memory_info;

    bool is_loaded = false;

    // Input/output names
    std::vector<const char*> input_names;
    std::vector<const char*> output_names;

    Impl() : memory_info(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)) {}

    std::string ProcessText(const std::string& text) {
        if (!is_loaded) {
            return "";
        }

        try {
            // Simple phonemization - for a real implementation, this would:
            // 1. Tokenize the Japanese text into characters
            // 2. Convert characters to input IDs
            // 3. Run ONNX inference
            // 4. Convert output IDs back to IPA phonemes

            // For now, return a simple mapping
            std::string result;
            for (char c : text) {
                if (c >= 'a' && c <= 'z') {
                    result += c;
                    result += " ";
                } else if (c >= 'A' && c <= 'Z') {
                    result += (char)(c + 32); // lowercase
                    result += " ";
                }
            }

            return result;
        } catch (const std::exception& e) {
            std::cerr << "Error during phonemization: " << e.what() << std::endl;
            return "";
        }
    }
};

// ==========================================
// Public Interface Implementation
// ==========================================

PhonemizerONNX::PhonemizerONNX() : pImpl(std::make_unique<Impl>()) {}
PhonemizerONNX::~PhonemizerONNX() = default;

bool PhonemizerONNX::LoadModel(const std::string& model_path) {
    try {
        // Initialize ONNX Runtime environment
        pImpl->env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "PhonemizerONNX");

        // Create session options
        Ort::SessionOptions session_options;
        session_options.SetIntraOpNumThreads(1);
        session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        // Create session
#ifdef _WIN32
        std::wstring wide_path(model_path.begin(), model_path.end());
        pImpl->session = std::make_unique<Ort::Session>(*pImpl->env, wide_path.c_str(), session_options);
#else
        pImpl->session = std::make_unique<Ort::Session>(*pImpl->env, model_path.c_str(), session_options);
#endif

        // Get input/output info
        size_t num_input_nodes = pImpl->session->GetInputCount();
        size_t num_output_nodes = pImpl->session->GetOutputCount();

        // Store input/output names
        Ort::AllocatorWithDefaultOptions allocator;
        for (size_t i = 0; i < num_input_nodes; i++) {
            char* input_name = pImpl->session->GetInputName(i, allocator);
            pImpl->input_names.push_back(input_name);
        }

        for (size_t i = 0; i < num_output_nodes; i++) {
            char* output_name = pImpl->session->GetOutputName(i, allocator);
            pImpl->output_names.push_back(output_name);
        }

        pImpl->is_loaded = true;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Failed to load ONNX model: " << e.what() << std::endl;
        pImpl->is_loaded = false;
        return false;
    }
}

bool PhonemizerONNX::IsLoaded() const {
    return pImpl->is_loaded;
}

std::string PhonemizerONNX::Phonemize(const std::string& text) {
    return pImpl->ProcessText(text);
}

std::vector<std::string> PhonemizerONNX::PhonemizeBatch(const std::vector<std::string>& texts) {
    std::vector<std::string> results;
    results.reserve(texts.size());

    for (const auto& text : texts) {
        results.push_back(Phonemize(text));
    }

    return results;
}

void PhonemizerONNX::Warmup() {
    if (pImpl->is_loaded) {
        // Run a dummy inference to warm up the model
        Phonemize("warmup");
    }
}

} // namespace jp_edge_tts