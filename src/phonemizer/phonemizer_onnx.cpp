/**
 * @file phonemizer_onnx.cpp
 * @brief Implementation of ONNX-based phonemizer for G2P conversion
 * @author D Everett Hinton
 * @date 2025
 *
 * @copyright MIT License
 */

#include "jp_edge_tts/phonemizer/phonemizer_onnx.h"
#include "jp_edge_tts/utils/file_utils.h"
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

class PhonemizeONNX::Impl {
public:
    std::unique_ptr<Ort::Session> session;
    std::unique_ptr<Ort::Env> env;
    Ort::MemoryInfo memory_info;
    
    // Model input/output info
    std::vector<std::string> input_names;
    std::vector<std::string> output_names;
    std::vector<const char*> input_names_ptr;
    std::vector<const char*> output_names_ptr;
    
    // Character to ID mapping for input encoding
    std::unordered_map<char32_t, int> char_to_id;
    std::unordered_map<int, std::string> id_to_phoneme;
    
    int pad_id = 0;
    int unk_id = 1;
    int bos_id = 2;
    int eos_id = 3;
    
    size_t max_input_length = 128;
    size_t max_output_length = 256;
    bool is_initialized = false;

    Impl() : memory_info(Ort::MemoryInfo::CreateCpu(
                         OrtAllocatorType::OrtArenaAllocator,
                         OrtMemType::OrtMemTypeDefault)) {}

    Status InitializeCharVocab() {
        // Initialize basic character vocabulary
        // This should ideally be loaded from model metadata
        
        // Special tokens
        char_to_id[U'<PAD>'] = pad_id;
        char_to_id[U'<UNK>'] = unk_id;
        char_to_id[U'<S>'] = bos_id;
        char_to_id[U'</S>'] = eos_id;
        
        // Japanese Hiragana (あ-ん)
        int id = 4;
        for (char32_t c = U'あ'; c <= U'ん'; c++) {
            char_to_id[c] = id++;
        }
        
        // Japanese Katakana (ア-ン)
        for (char32_t c = U'ア'; c <= U'ン'; c++) {
            char_to_id[c] = id++;
        }
        
        // Common Kanji (simplified - should be loaded from vocab file)
        std::u32string common_kanji = U"一二三四五六七八九十百千万円時日月年";
        for (char32_t c : common_kanji) {
            char_to_id[c] = id++;
        }
        
        // ASCII characters
        for (char c = ' '; c <= '~'; c++) {
            char_to_id[static_cast<char32_t>(c)] = id++;
        }
        
        // Japanese punctuation
        std::u32string jp_punct = U"。、！？「」『』（）・ー";
        for (char32_t c : jp_punct) {
            char_to_id[c] = id++;
        }
        
        return Status::OK;
    }

    Status InitializePhonemVocab() {
        // Initialize phoneme output vocabulary
        // This should ideally be loaded from model metadata
        
        id_to_phoneme[0] = "";
        id_to_phoneme[1] = "";
        id_to_phoneme[2] = "";
        id_to_phoneme[3] = "";
        
        // Japanese phonemes (simplified IPA)
        std::vector<std::string> phonemes = {
            "a", "i", "u", "e", "o",
            "ka", "ki", "ku", "ke", "ko",
            "ga", "gi", "gu", "ge", "go",
            "sa", "si", "su", "se", "so",
            "za", "zi", "zu", "ze", "zo",
            "ta", "ti", "tu", "te", "to",
            "da", "di", "du", "de", "do",
            "na", "ni", "nu", "ne", "no",
            "ha", "hi", "hu", "he", "ho",
            "ba", "bi", "bu", "be", "bo",
            "pa", "pi", "pu", "pe", "po",
            "ma", "mi", "mu", "me", "mo",
            "ya", "yu", "yo",
            "ra", "ri", "ru", "re", "ro",
            "wa", "wo", "n",
            "kya", "kyu", "kyo",
            "gya", "gyu", "gyo",
            "sha", "shu", "sho",
            "ja", "ju", "jo",
            "cha", "chu", "cho",
            "nya", "nyu", "nyo",
            "hya", "hyu", "hyo",
            "bya", "byu", "byo",
            "pya", "pyu", "pyo",
            "mya", "myu", "myo",
            "rya", "ryu", "ryo",
            "q",  // Glottal stop
            ":",  // Length mark
            ".",  // Pause
            ",",  // Short pause
            "!",  // Emphasis
            "?",  // Question
        };
        
        int id = 4;
        for (const auto& phoneme : phonemes) {
            id_to_phoneme[id++] = phoneme;
        }
        
        return Status::OK;
    }

    std::vector<int> EncodeText(const std::string& text) {
        // Convert UTF-8 to UTF-32 for easier character handling
        std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
        std::u32string utf32_text = converter.from_bytes(text);
        
        std::vector<int> encoded;
        encoded.reserve(utf32_text.length() + 2);
        
        // Add BOS token
        encoded.push_back(bos_id);
        
        // Encode each character
        for (char32_t c : utf32_text) {
            auto it = char_to_id.find(c);
            if (it != char_to_id.end()) {
                encoded.push_back(it->second);
            } else {
                encoded.push_back(unk_id);
            }
        }
        
        // Add EOS token
        encoded.push_back(eos_id);
        
        // Pad or truncate to max length
        if (encoded.size() > max_input_length) {
            encoded.resize(max_input_length);
            encoded[max_input_length - 1] = eos_id;
        } else {
            while (encoded.size() < max_input_length) {
                encoded.push_back(pad_id);
            }
        }
        
        return encoded;
    }

    std::string DecodePhonemes(const std::vector<int>& phoneme_ids) {
        std::vector<std::string> phonemes;
        
        for (int id : phoneme_ids) {
            // Skip special tokens
            if (id == pad_id || id == bos_id || id == eos_id) {
                continue;
            }
            
            // Stop at EOS or padding
            if (id == eos_id || id == pad_id) {
                break;
            }
            
            auto it = id_to_phoneme.find(id);
            if (it != id_to_phoneme.end() && !it->second.empty()) {
                phonemes.push_back(it->second);
            }
        }
        
        // Join phonemes with spaces
        std::string result;
        for (size_t i = 0; i < phonemes.size(); i++) {
            if (i > 0) result += " ";
            result += phonemes[i];
        }
        
        return result;
    }
};

// ==========================================
// Public Interface Implementation
// ==========================================

PhonemizeONNX::PhonemizeONNX() : pImpl(std::make_unique<Impl>()) {}
PhonemizeONNX::~PhonemizeONNX() = default;
PhonemizeONNX::PhonemizeONNX(PhonemizeONNX&&) noexcept = default;
PhonemizeONNX& PhonemizeONNX::operator=(PhonemizeONNX&&) noexcept = default;

Status PhonemizeONNX::Initialize(const std::string& model_path) {
    try {
        // Check if model file exists
        if (!FileUtils::Exists(model_path)) {
            std::cerr << "Phonemizer model not found: " << model_path << std::endl;
            return Status::ERROR_FILE_NOT_FOUND;
        }
        
        // Initialize ONNX Runtime environment
        pImpl->env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "PhonemizeONNX");
        
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
        
        // Get input/output names
        Ort::AllocatorWithDefaultOptions allocator;
        
        size_t num_inputs = pImpl->session->GetInputCount();
        pImpl->input_names.reserve(num_inputs);
        pImpl->input_names_ptr.reserve(num_inputs);
        
        for (size_t i = 0; i < num_inputs; i++) {
            auto name = pImpl->session->GetInputNameAllocated(i, allocator);
            pImpl->input_names.push_back(name.get());
            pImpl->input_names_ptr.push_back(pImpl->input_names.back().c_str());
        }
        
        size_t num_outputs = pImpl->session->GetOutputCount();
        pImpl->output_names.reserve(num_outputs);
        pImpl->output_names_ptr.reserve(num_outputs);
        
        for (size_t i = 0; i < num_outputs; i++) {
            auto name = pImpl->session->GetOutputNameAllocated(i, allocator);
            pImpl->output_names.push_back(name.get());
            pImpl->output_names_ptr.push_back(pImpl->output_names.back().c_str());
        }
        
        // Initialize vocabularies
        pImpl->InitializeCharVocab();
        pImpl->InitializePhonemVocab();
        
        pImpl->is_initialized = true;
        return Status::OK;
        
    } catch (const Ort::Exception& e) {
        std::cerr << "ONNX Runtime error: " << e.what() << std::endl;
        return Status::ERROR_INITIALIZATION_FAILED;
    } catch (const std::exception& e) {
        std::cerr << "Error initializing phonemizer: " << e.what() << std::endl;
        return Status::ERROR_INITIALIZATION_FAILED;
    }
}

std::string PhonemizeONNX::Phonemize(const std::string& text) {
    if (!pImpl->is_initialized || text.empty()) {
        return "";
    }
    
    try {
        // Encode input text
        std::vector<int> input_ids = pImpl->EncodeText(text);
        
        // Prepare input tensor
        std::vector<int64_t> input_shape = {1, static_cast<int64_t>(input_ids.size())};
        std::vector<int64_t> input_data(input_ids.begin(), input_ids.end());
        
        Ort::Value input_tensor = Ort::Value::CreateTensor<int64_t>(
            pImpl->memory_info,
            input_data.data(),
            input_data.size(),
            input_shape.data(),
            input_shape.size()
        );
        
        // Run inference
        auto output_tensors = pImpl->session->Run(
            Ort::RunOptions{nullptr},
            pImpl->input_names_ptr.data(),
            &input_tensor,
            1,
            pImpl->output_names_ptr.data(),
            pImpl->output_names_ptr.size()
        );
        
        // Process output
        if (!output_tensors.empty()) {
            auto& output_tensor = output_tensors[0];
            int64_t* output_data = output_tensor.GetTensorMutableData<int64_t>();
            auto output_shape = output_tensor.GetTensorTypeAndShapeInfo().GetShape();
            
            // Convert output to phoneme IDs
            size_t output_length = output_shape.back();
            std::vector<int> phoneme_ids;
            
            for (size_t i = 0; i < output_length; i++) {
                phoneme_ids.push_back(static_cast<int>(output_data[i]));
            }
            
            // Decode phonemes
            return pImpl->DecodePhonemes(phoneme_ids);
        }
        
        return "";
        
    } catch (const Ort::Exception& e) {
        std::cerr << "ONNX Runtime error during phonemization: " << e.what() << std::endl;
        return "";
    } catch (const std::exception& e) {
        std::cerr << "Error during phonemization: " << e.what() << std::endl;
        return "";
    }
}

std::vector<std::string> PhonemizeONNX::BatchPhonemize(
    const std::vector<std::string>& texts) {
    
    std::vector<std::string> results;
    results.reserve(texts.size());
    
    // Process each text individually
    // TODO: Implement true batch processing for efficiency
    for (const auto& text : texts) {
        results.push_back(Phonemize(text));
    }
    
    return results;
}

bool PhonemizeONNX::IsInitialized() const {
    return pImpl->is_initialized;
}

Status PhonemizeONNX::LoadVocabulary(const std::string& char_vocab_path,
                                     const std::string& phoneme_vocab_path) {
    // TODO: Implement loading vocabularies from files
    // For now, using hardcoded vocabularies
    return Status::OK;
}

void PhonemizeONNX::SetMaxInputLength(size_t length) {
    pImpl->max_input_length = length;
}

void PhonemizeONNX::SetMaxOutputLength(size_t length) {
    pImpl->max_output_length = length;
}

} // namespace jp_edge_tts