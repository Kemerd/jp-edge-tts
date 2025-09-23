/**
 * @file session_manager.cpp
 * @brief Implementation of ONNX Runtime session management
 * @author D Everett Hinton
 * @date 2025
 *
 * @copyright MIT License
 */

#include "jp_edge_tts/core/session_manager.h"
#include <onnxruntime_cxx_api.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <numeric>

namespace jp_edge_tts {

// ==========================================
// Private Implementation
// ==========================================

class SessionManager::Impl {
public:
    // ONNX Runtime components
    std::unique_ptr<Ort::Env> env;
    std::unique_ptr<Ort::SessionOptions> session_options;
    std::unique_ptr<Ort::Session> session;
    std::unique_ptr<Ort::AllocatorWithDefaultOptions> allocator;
    std::unique_ptr<Ort::MemoryInfo> memory_info;

    // Model information
    std::vector<std::string> input_names;
    std::vector<std::string> output_names;
    std::vector<std::vector<int64_t>> input_shapes;
    std::vector<std::vector<int64_t>> output_shapes;

    // Statistics
    mutable std::mutex stats_mutex;
    size_t total_inferences = 0;
    double total_latency_ms = 0;
    double min_latency_ms = std::numeric_limits<double>::max();
    double max_latency_ms = 0;

    // Configuration
    bool use_gpu = false;
    int num_threads = 0;  // 0 = auto
    bool loaded = false;

    Impl() {
        // Create ONNX Runtime environment
        env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "jp_edge_tts");
        allocator = std::make_unique<Ort::AllocatorWithDefaultOptions>();
        memory_info = std::make_unique<Ort::MemoryInfo>(
            Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)
        );
    }

    ~Impl() {
        // Cleanup happens automatically with unique_ptrs
    }

    bool LoadModel(const std::string& model_path) {
        try {
            // Create session options
            session_options = std::make_unique<Ort::SessionOptions>();

            // Configure for high performance
            session_options->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

            // Set thread count
            if (num_threads == 0) {
                // Use all available cores
                session_options->SetIntraOpNumThreads(0);
                session_options->SetInterOpNumThreads(0);
            } else {
                session_options->SetIntraOpNumThreads(num_threads);
                session_options->SetInterOpNumThreads(num_threads);
            }

            // Enable parallel execution
            session_options->SetExecutionMode(ExecutionMode::ORT_PARALLEL);

            // GPU configuration
            if (use_gpu) {
                #ifdef USE_CUDA
                OrtCUDAProviderOptions cuda_options;
                cuda_options.device_id = 0;
                cuda_options.arena_extend_strategy = 0;
                cuda_options.gpu_mem_limit = SIZE_MAX;
                cuda_options.cudnn_conv_algo_search = OrtCudnnConvAlgoSearch::EXHAUSTIVE;
                cuda_options.do_copy_in_default_stream = 1;

                session_options->AppendExecutionProvider_CUDA(cuda_options);
                #endif
            }

            // Create session
            #ifdef _WIN32
            std::wstring wide_path(model_path.begin(), model_path.end());
            session = std::make_unique<Ort::Session>(*env, wide_path.c_str(), *session_options);
            #else
            session = std::make_unique<Ort::Session>(*env, model_path.c_str(), *session_options);
            #endif

            // Get input and output information
            ExtractModelInfo();

            loaded = true;
            return true;

        } catch (const Ort::Exception& e) {
            std::cerr << "ONNX Runtime error: " << e.what() << std::endl;
            loaded = false;
            return false;
        } catch (const std::exception& e) {
            std::cerr << "Error loading model: " << e.what() << std::endl;
            loaded = false;
            return false;
        }
    }

    bool LoadModelFromMemory(const void* model_data, size_t model_size) {
        try {
            // Create session options (same as file loading)
            session_options = std::make_unique<Ort::SessionOptions>();
            session_options->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

            if (num_threads == 0) {
                session_options->SetIntraOpNumThreads(0);
                session_options->SetInterOpNumThreads(0);
            } else {
                session_options->SetIntraOpNumThreads(num_threads);
                session_options->SetInterOpNumThreads(num_threads);
            }

            session_options->SetExecutionMode(ExecutionMode::ORT_PARALLEL);

            // Create session from memory
            session = std::make_unique<Ort::Session>(
                *env, model_data, model_size, *session_options
            );

            // Get input and output information
            ExtractModelInfo();

            loaded = true;
            return true;

        } catch (const Ort::Exception& e) {
            std::cerr << "ONNX Runtime error: " << e.what() << std::endl;
            loaded = false;
            return false;
        }
    }

    void ExtractModelInfo() {
        if (!session) return;

        // Get input names and shapes
        size_t num_inputs = session->GetInputCount();
        input_names.clear();
        input_shapes.clear();

        for (size_t i = 0; i < num_inputs; i++) {
            // Get input name
            auto name = session->GetInputNameAllocated(i, *allocator);
            input_names.push_back(name.get());

            // Get input shape
            auto type_info = session->GetInputTypeInfo(i);
            auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
            input_shapes.push_back(tensor_info.GetShape());
        }

        // Get output names and shapes
        size_t num_outputs = session->GetOutputCount();
        output_names.clear();
        output_shapes.clear();

        for (size_t i = 0; i < num_outputs; i++) {
            // Get output name
            auto name = session->GetOutputNameAllocated(i, *allocator);
            output_names.push_back(name.get());

            // Get output shape
            auto type_info = session->GetOutputTypeInfo(i);
            auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
            output_shapes.push_back(tensor_info.GetShape());
        }
    }

    std::vector<float> RunInference(
        const std::vector<int>& tokens,
        const std::vector<float>& style_vector,
        float speed,
        float pitch
    ) {
        if (!loaded || !session) {
            return {};
        }

        auto start = std::chrono::high_resolution_clock::now();

        try {
            // Prepare input tensors
            std::vector<Ort::Value> input_tensors;

            // 1. Tokens tensor (shape: [1, sequence_length])
            std::vector<int64_t> token_shape = {1, static_cast<int64_t>(tokens.size())};
            std::vector<int64_t> token_data(tokens.begin(), tokens.end());

            input_tensors.push_back(
                Ort::Value::CreateTensor<int64_t>(
                    *memory_info,
                    token_data.data(),
                    token_data.size(),
                    token_shape.data(),
                    token_shape.size()
                )
            );

            // 2. Style vector tensor (shape: [1, style_dim])
            std::vector<int64_t> style_shape = {1, static_cast<int64_t>(style_vector.size())};
            auto style_data = style_vector;  // Copy to ensure contiguous memory

            input_tensors.push_back(
                Ort::Value::CreateTensor<float>(
                    *memory_info,
                    style_data.data(),
                    style_data.size(),
                    style_shape.data(),
                    style_shape.size()
                )
            );

            // 3. Speed tensor (shape: [1])
            std::vector<float> speed_data = {speed};
            std::vector<int64_t> speed_shape = {1};

            input_tensors.push_back(
                Ort::Value::CreateTensor<float>(
                    *memory_info,
                    speed_data.data(),
                    speed_data.size(),
                    speed_shape.data(),
                    speed_shape.size()
                )
            );

            // If model expects pitch (4th input)
            if (input_names.size() > 3) {
                std::vector<float> pitch_data = {pitch};
                std::vector<int64_t> pitch_shape = {1};

                input_tensors.push_back(
                    Ort::Value::CreateTensor<float>(
                        *memory_info,
                        pitch_data.data(),
                        pitch_data.size(),
                        pitch_shape.data(),
                        pitch_shape.size()
                    )
                );
            }

            // Prepare input/output names
            std::vector<const char*> input_names_raw;
            for (const auto& name : input_names) {
                input_names_raw.push_back(name.c_str());
            }

            std::vector<const char*> output_names_raw;
            for (const auto& name : output_names) {
                output_names_raw.push_back(name.c_str());
            }

            // Run inference
            auto output_tensors = session->Run(
                Ort::RunOptions{nullptr},
                input_names_raw.data(),
                input_tensors.data(),
                input_tensors.size(),
                output_names_raw.data(),
                output_names_raw.size()
            );

            // Extract audio samples from output
            if (!output_tensors.empty()) {
                auto& audio_tensor = output_tensors[0];
                float* audio_data = audio_tensor.GetTensorMutableData<float>();

                auto shape_info = audio_tensor.GetTensorTypeAndShapeInfo();
                auto shape = shape_info.GetShape();

                // Calculate total size
                size_t total_size = 1;
                for (auto dim : shape) {
                    total_size *= dim;
                }

                // Copy audio samples
                std::vector<float> audio_samples(audio_data, audio_data + total_size);

                // Update statistics
                auto end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
                double latency_ms = duration.count() / 1000.0;

                {
                    std::lock_guard<std::mutex> lock(stats_mutex);
                    total_inferences++;
                    total_latency_ms += latency_ms;
                    min_latency_ms = std::min(min_latency_ms, latency_ms);
                    max_latency_ms = std::max(max_latency_ms, latency_ms);
                }

                return audio_samples;
            }

        } catch (const Ort::Exception& e) {
            std::cerr << "ONNX Runtime inference error: " << e.what() << std::endl;
        }

        return {};
    }

    void Warmup() {
        if (!loaded) return;

        // Create dummy input for warmup
        std::vector<int> dummy_tokens(10, 1);  // 10 tokens
        std::vector<float> dummy_style(128, 0.5f);  // 128-dim style vector

        // Run warmup inference
        RunInference(dummy_tokens, dummy_style, 1.0f, 1.0f);

        // Reset statistics after warmup
        total_inferences = 0;
        total_latency_ms = 0;
        min_latency_ms = std::numeric_limits<double>::max();
        max_latency_ms = 0;
    }
};

// ==========================================
// Public Interface Implementation
// ==========================================

SessionManager::SessionManager() : pImpl(std::make_unique<Impl>()) {}
SessionManager::~SessionManager() = default;
SessionManager::SessionManager(SessionManager&&) noexcept = default;
SessionManager& SessionManager::operator=(SessionManager&&) noexcept = default;

bool SessionManager::LoadModel(const std::string& model_path) {
    return pImpl->LoadModel(model_path);
}

bool SessionManager::LoadModelFromMemory(const void* model_data, size_t model_size) {
    return pImpl->LoadModelFromMemory(model_data, model_size);
}

bool SessionManager::IsLoaded() const {
    return pImpl->loaded;
}

std::vector<float> SessionManager::RunInference(
    const std::vector<int>& tokens,
    const std::vector<float>& style_vector,
    float speed,
    float pitch
) {
    return pImpl->RunInference(tokens, style_vector, speed, pitch);
}

std::vector<std::vector<float>> SessionManager::RunBatchInference(
    const std::vector<std::vector<int>>& batch_tokens,
    const std::vector<std::vector<float>>& style_vectors,
    const std::vector<float>& speeds
) {
    std::vector<std::vector<float>> results;

    // For now, process sequentially
    // TODO: Implement true batch processing if model supports it
    for (size_t i = 0; i < batch_tokens.size(); i++) {
        float speed = (i < speeds.size()) ? speeds[i] : 1.0f;
        auto audio = RunInference(batch_tokens[i], style_vectors[i], speed, 1.0f);
        results.push_back(audio);
    }

    return results;
}

std::vector<std::pair<std::string, std::vector<int64_t>>> SessionManager::GetInputInfo() const {
    std::vector<std::pair<std::string, std::vector<int64_t>>> info;
    for (size_t i = 0; i < pImpl->input_names.size(); i++) {
        info.emplace_back(pImpl->input_names[i], pImpl->input_shapes[i]);
    }
    return info;
}

std::vector<std::pair<std::string, std::vector<int64_t>>> SessionManager::GetOutputInfo() const {
    std::vector<std::pair<std::string, std::vector<int64_t>>> info;
    for (size_t i = 0; i < pImpl->output_names.size(); i++) {
        info.emplace_back(pImpl->output_names[i], pImpl->output_shapes[i]);
    }
    return info;
}

void SessionManager::SetNumThreads(int num_threads) {
    pImpl->num_threads = num_threads;
}

void SessionManager::SetUseGPU(bool enable) {
    pImpl->use_gpu = enable;
}

SessionManager::SessionStats SessionManager::GetStats() const {
    std::lock_guard<std::mutex> lock(pImpl->stats_mutex);

    SessionStats stats;
    stats.total_inferences = pImpl->total_inferences;
    stats.average_latency_ms = (pImpl->total_inferences > 0) ?
                               pImpl->total_latency_ms / pImpl->total_inferences : 0;
    stats.min_latency_ms = (pImpl->total_inferences > 0) ?
                           pImpl->min_latency_ms : 0;
    stats.max_latency_ms = pImpl->max_latency_ms;
    stats.memory_usage_bytes = 0;  // TODO: Implement memory tracking

    return stats;
}

void SessionManager::ResetStats() {
    std::lock_guard<std::mutex> lock(pImpl->stats_mutex);
    pImpl->total_inferences = 0;
    pImpl->total_latency_ms = 0;
    pImpl->min_latency_ms = std::numeric_limits<double>::max();
    pImpl->max_latency_ms = 0;
}

void SessionManager::Warmup() {
    pImpl->Warmup();
}

} // namespace jp_edge_tts