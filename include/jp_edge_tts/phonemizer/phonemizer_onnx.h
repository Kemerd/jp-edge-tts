/**
 * @file phonemizer_onnx.h
 * @brief ONNX-based phonemizer for unknown words
 * @author D Everett Hinton
 * @date 2025
 *
 * @copyright MIT License
 */

#ifndef JP_EDGE_TTS_PHONEMIZER_ONNX_H
#define JP_EDGE_TTS_PHONEMIZER_ONNX_H

#include <string>
#include <vector>
#include <memory>

namespace jp_edge_tts {

/**
 * @class PhonemizerONNX
 * @brief Neural network-based phonemizer using DeepPhonemizer ONNX model
 */
class PhonemizerONNX {
public:
    PhonemizerONNX();
    ~PhonemizerONNX();

    /**
     * @brief Load ONNX model from file
     * @param model_path Path to ONNX model
     * @return true if successful
     */
    bool LoadModel(const std::string& model_path);

    /**
     * @brief Check if model is loaded
     * @return true if ready
     */
    bool IsLoaded() const;

    /**
     * @brief Convert text to phonemes using model
     * @param text Japanese text
     * @return IPA phonemes
     */
    std::string Phonemize(const std::string& text);

    /**
     * @brief Batch phonemization
     * @param texts Vector of texts
     * @return Vector of phoneme strings
     */
    std::vector<std::string> PhonemizeBatch(const std::vector<std::string>& texts);

    /**
     * @brief Warmup the model
     */
    void Warmup();

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace jp_edge_tts

#endif // JP_EDGE_TTS_PHONEMIZER_ONNX_H