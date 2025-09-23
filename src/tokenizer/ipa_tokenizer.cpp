/**
 * @file ipa_tokenizer.cpp
 * @brief Implementation of IPA phoneme tokenization
 * @author D Everett Hinton
 * @date 2025
 *
 * @copyright MIT License
 */

#include "jp_edge_tts/tokenizer/ipa_tokenizer.h"
#include "jp_edge_tts/utils/string_utils.h"
#include <algorithm>
#include <sstream>
#include <cctype>

namespace jp_edge_tts {

// ==========================================
// Private Implementation
// ==========================================

class IPATokenizer::Impl {
public:
    VocabManager vocab_manager;
    bool is_initialized;
    
    // Special tokens
    std::string pad_token;
    std::string unk_token;
    std::string bos_token;
    std::string eos_token;
    
    // Token IDs
    int pad_id;
    int unk_id;
    int bos_id;
    int eos_id;

    Impl() : is_initialized(false),
             pad_token("<pad>"),
             unk_token("<unk>"),
             bos_token("<s>"),
             eos_token("</s>"),
             pad_id(0),
             unk_id(1),
             bos_id(2),
             eos_id(3) {}

    std::vector<std::string> SplitPhonemes(const std::string& phonemes) {
        // Split phonemes by whitespace
        std::vector<std::string> result;
        std::stringstream ss(phonemes);
        std::string token;
        
        while (ss >> token) {
            if (!token.empty()) {
                result.push_back(token);
            }
        }
        
        return result;
    }

    std::string NormalizePhoneme(const std::string& phoneme) {
        // Basic normalization for IPA symbols
        std::string normalized = phoneme;
        
        // Convert to lowercase for consistency
        // Note: This is simplified - real IPA has complex Unicode
        std::transform(normalized.begin(), normalized.end(),
                      normalized.begin(), ::tolower);
        
        // Remove any surrounding whitespace
        normalized = StringUtils::Trim(normalized);
        
        return normalized;
    }

    std::vector<int> AddSpecialTokens(const std::vector<int>& tokens,
                                      bool add_bos,
                                      bool add_eos) {
        std::vector<int> result;
        
        // Add beginning of sequence token
        if (add_bos) {
            result.push_back(bos_id);
        }
        
        // Add main tokens
        result.insert(result.end(), tokens.begin(), tokens.end());
        
        // Add end of sequence token
        if (add_eos) {
            result.push_back(eos_id);
        }
        
        return result;
    }
};

// ==========================================
// Public Interface Implementation
// ==========================================

IPATokenizer::IPATokenizer() : pImpl(std::make_unique<Impl>()) {}
IPATokenizer::~IPATokenizer() = default;
IPATokenizer::IPATokenizer(IPATokenizer&&) noexcept = default;
IPATokenizer& IPATokenizer::operator=(IPATokenizer&&) noexcept = default;

Status IPATokenizer::Initialize(const std::string& vocab_path) {
    // Load vocabulary
    Status status = pImpl->vocab_manager.LoadVocabulary(vocab_path);
    if (status != Status::OK) {
        return status;
    }
    
    // Verify special tokens exist in vocabulary
    pImpl->pad_id = pImpl->vocab_manager.GetTokenId(pImpl->pad_token);
    pImpl->unk_id = pImpl->vocab_manager.GetTokenId(pImpl->unk_token);
    pImpl->bos_id = pImpl->vocab_manager.GetTokenId(pImpl->bos_token);
    pImpl->eos_id = pImpl->vocab_manager.GetTokenId(pImpl->eos_token);
    
    // Use defaults if not found
    if (pImpl->pad_id == -1) pImpl->pad_id = 0;
    if (pImpl->unk_id == -1) pImpl->unk_id = 1;
    if (pImpl->bos_id == -1) pImpl->bos_id = 2;
    if (pImpl->eos_id == -1) pImpl->eos_id = 3;
    
    pImpl->is_initialized = true;
    return Status::OK;
}

std::vector<int> IPATokenizer::Tokenize(const std::string& phonemes) {
    if (!pImpl->is_initialized) {
        return {};
    }
    
    // Split phonemes into individual tokens
    auto phoneme_list = pImpl->SplitPhonemes(phonemes);
    
    // Convert phonemes to token IDs
    std::vector<int> tokens;
    for (const auto& phoneme : phoneme_list) {
        // Normalize the phoneme
        std::string normalized = pImpl->NormalizePhoneme(phoneme);
        
        // Get token ID from vocabulary
        int token_id = pImpl->vocab_manager.GetTokenId(normalized);
        
        // Use unknown token if not found
        if (token_id == -1) {
            token_id = pImpl->unk_id;
        }
        
        tokens.push_back(token_id);
    }
    
    return tokens;
}

std::vector<int> IPATokenizer::TokenizeWithSpecialTokens(
    const std::string& phonemes,
    bool add_bos,
    bool add_eos) {
    
    // Get base tokens
    auto tokens = Tokenize(phonemes);
    
    // Add special tokens
    return pImpl->AddSpecialTokens(tokens, add_bos, add_eos);
}

std::string IPATokenizer::Detokenize(const std::vector<int>& tokens) {
    if (!pImpl->is_initialized) {
        return "";
    }
    
    std::vector<std::string> phonemes;
    
    for (int token_id : tokens) {
        // Skip special tokens when detokenizing
        if (token_id == pImpl->pad_id ||
            token_id == pImpl->bos_id ||
            token_id == pImpl->eos_id) {
            continue;
        }
        
        // Get phoneme from vocabulary
        std::string phoneme = pImpl->vocab_manager.GetToken(token_id);
        
        // Skip unknown tokens
        if (phoneme.empty() || phoneme == pImpl->unk_token) {
            continue;
        }
        
        phonemes.push_back(phoneme);
    }
    
    // Join phonemes with spaces
    return StringUtils::Join(phonemes, " ");
}

std::vector<std::vector<int>> IPATokenizer::BatchTokenize(
    const std::vector<std::string>& phoneme_list,
    bool add_special_tokens) {
    
    std::vector<std::vector<int>> batch;
    
    for (const auto& phonemes : phoneme_list) {
        if (add_special_tokens) {
            batch.push_back(TokenizeWithSpecialTokens(phonemes, true, true));
        } else {
            batch.push_back(Tokenize(phonemes));
        }
    }
    
    return batch;
}

std::vector<std::vector<int>> IPATokenizer::PadBatch(
    const std::vector<std::vector<int>>& batch,
    size_t max_length,
    bool pad_to_max) {
    
    if (batch.empty()) {
        return batch;
    }
    
    // Find the maximum length in the batch
    size_t batch_max = 0;
    for (const auto& tokens : batch) {
        batch_max = std::max(batch_max, tokens.size());
    }
    
    // Use provided max_length or batch max
    size_t target_length = pad_to_max ? max_length : 
                          (max_length > 0 ? std::min(max_length, batch_max) : batch_max);
    
    // Pad all sequences to target length
    std::vector<std::vector<int>> padded;
    for (const auto& tokens : batch) {
        std::vector<int> padded_tokens = tokens;
        
        // Truncate if too long
        if (padded_tokens.size() > target_length) {
            padded_tokens.resize(target_length);
        }
        // Pad if too short
        else if (padded_tokens.size() < target_length) {
            size_t pad_count = target_length - padded_tokens.size();
            for (size_t i = 0; i < pad_count; i++) {
                padded_tokens.push_back(pImpl->pad_id);
            }
        }
        
        padded.push_back(padded_tokens);
    }
    
    return padded;
}

int IPATokenizer::GetPadTokenId() const {
    return pImpl->pad_id;
}

int IPATokenizer::GetUnkTokenId() const {
    return pImpl->unk_id;
}

int IPATokenizer::GetBosTokenId() const {
    return pImpl->bos_id;
}

int IPATokenizer::GetEosTokenId() const {
    return pImpl->eos_id;
}

size_t IPATokenizer::GetVocabSize() const {
    return pImpl->vocab_manager.GetVocabularySize();
}

bool IPATokenizer::IsInitialized() const {
    return pImpl->is_initialized;
}

VocabManager& IPATokenizer::GetVocabManager() {
    return pImpl->vocab_manager;
}

const VocabManager& IPATokenizer::GetVocabManager() const {
    return pImpl->vocab_manager;
}

} // namespace jp_edge_tts