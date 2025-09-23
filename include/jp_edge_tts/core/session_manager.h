/**
 * @file session_manager.h
 * @brief ONNX Runtime session management for model inference
 * D Everett Hinton
 * @date 2025
 *
 * @copyright MIT License
 */

#ifndef JP_EDGE_TTS_SESSION_MANAGER_H
#define JP_EDGE_TTS_SESSION_MANAGER_H

#include "jp_edge_tts/types.h"
#include <memory>
#include <vector>
#include <string>
#include <mutex>

namespace jp_edge_tts {

/**
 * @class SessionManager
 * @brief Manages ONNX Runtime sessions for model inference
 *
 * @details Handles loading ONNX models, creating inference sessions,
 * and running forward passes with proper tensor management.
 */
class SessionManager {
public:
    /**
     * @brief Constructor
     */
    SessionManager();

    /**
     * @brief Destructor
     */
    ~SessionManager();

    // Disable copy, enable move
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;
    SessionManager(SessionManager&&) noexcept;
    SessionManager& operator=(SessionManager&&) noexcept;

    /**
     * @brief Load ONNX model from file
     *
     * @param model_path Path to ONNX model file
     * @return true if successful
     */
    bool LoadModel(const std::string& model_path);

    /**
     * @brief Load ONNX model from memory
     *
     * @param model_data Model data in memory
     * @param model_size Size of model data
     * @return true if successful
     */
    bool LoadModelFromMemory(const void* model_data, size_t model_size);

    /**
     * @brief Check if model is loaded
     * @return true if model is ready for inference
     */
    bool IsLoaded() const;

    /**
     * @brief Run TTS inference
     *
     * @param tokens Input token IDs
     * @param style_vector Voice style embedding
     * @param speed Speaking speed factor
     * @param pitch Pitch adjustment factor
     * @return Generated audio samples
     */
    std::vector<float> RunInference(
        const std::vector<int>& tokens,
        const std::vector<float>& style_vector,
        float speed = 1.0f,
        float pitch = 1.0f
    );

    /**
     * @brief Run batch inference for multiple inputs
     *
     * @param batch_tokens Batch of token sequences
     * @param style_vectors Batch of style vectors
     * @param speeds Speed factors for each input
     * @return Batch of generated audio samples
     */
    std::vector<std::vector<float>> RunBatchInference(
        const std::vector<std::vector<int>>& batch_tokens,
        const std::vector<std::vector<float>>& style_vectors,
        const std::vector<float>& speeds
    );

    /**
     * @brief Get model input information
     * @return Vector of input tensor names and shapes
     */
    std::vector<std::pair<std::string, std::vector<int64_t>>> GetInputInfo() const;

    /**
     * @brief Get model output information
     * @return Vector of output tensor names and shapes
     */
    std::vector<std::pair<std::string, std::vector<int64_t>>> GetOutputInfo() const;

    /**
     * @brief Set number of threads for inference
     * @param num_threads Number of threads (0 = auto)
     */
    void SetNumThreads(int num_threads);

    /**
     * @brief Enable/disable GPU acceleration
     * @param enable true to use GPU if available
     */
    void SetUseGPU(bool enable);

    /**
     * @brief Get session statistics
     */
    struct SessionStats {
        size_t total_inferences;
        double average_latency_ms;
        double min_latency_ms;
        double max_latency_ms;
        size_t memory_usage_bytes;
    };
    SessionStats GetStats() const;

    /**
     * @brief Reset statistics
     */
    void ResetStats();

    /**
     * @brief Warmup the model with dummy input
     */
    void Warmup();

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace jp_edge_tts

#endif // JP_EDGE_TTS_SESSION_MANAGER_H