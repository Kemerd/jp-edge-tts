/**
 * @file japanese_phonemizer.cpp
 * @brief Implementation of Japanese text to phoneme conversion
 * @author D Everett Hinton
 * @date 2025
 *
 * @copyright MIT License
 */

#include "jp_edge_tts/phonemizer/japanese_phonemizer.h"
#include "jp_edge_tts/utils/string_utils.h"
#include <algorithm>
#include <sstream>
#include <iostream>
#include <regex>

namespace jp_edge_tts {

// ==========================================
// Private Implementation
// ==========================================

class JapanesePhonemizer::Impl {
public:
    MeCabWrapper mecab;
    DictionaryLookup dictionary;
    PhonemizeONNX onnx_phonemizer;
    
    bool use_dictionary = true;
    bool use_onnx_fallback = true;
    bool is_initialized = false;
    
    // Statistics
    size_t dictionary_hits = 0;
    size_t onnx_fallbacks = 0;
    size_t total_words = 0;

    std::string ProcessMorpheme(const MorphemeInfo& morpheme) {
        total_words++;
        
        // First, try dictionary lookup with reading and POS
        if (use_dictionary) {
            auto result = dictionary.LookupWithReading(
                morpheme.surface,
                morpheme.reading,
                morpheme.pos
            );
            
            if (result.has_value()) {
                dictionary_hits++;
                return result.value();
            }
        }
        
        // Fallback to ONNX model if enabled
        if (use_onnx_fallback && onnx_phonemizer.IsInitialized()) {
            onnx_fallbacks++;
            std::string phonemes = onnx_phonemizer.Phonemize(morpheme.surface);
            if (!phonemes.empty()) {
                return phonemes;
            }
        }
        
        // Last resort: use reading if available
        if (!morpheme.reading.empty()) {
            return ConvertReadingToPhonemes(morpheme.reading);
        }
        
        // Final fallback: romanize the surface form
        return RomanizeText(morpheme.surface);
    }

    std::string ConvertReadingToPhonemes(const std::string& reading) {
        // Convert Katakana reading to phonemes
        // This is a simplified conversion - real implementation would be more complex
        
        std::string phonemes = reading;
        
        // Basic Katakana to phoneme mappings
        std::vector<std::pair<std::string, std::string>> mappings = {
            // Long vowels
            {"ー", ":"}, // Long vowel mark
            {"アア", "a:"}, {"イイ", "i:"}, {"ウウ", "u:"},
            {"エエ", "e:"}, {"オオ", "o:"},
            
            // Syllables
            {"ア", "a"}, {"イ", "i"}, {"ウ", "u"}, {"エ", "e"}, {"オ", "o"},
            {"カ", "ka"}, {"キ", "ki"}, {"ク", "ku"}, {"ケ", "ke"}, {"コ", "ko"},
            {"ガ", "ga"}, {"ギ", "gi"}, {"グ", "gu"}, {"ゲ", "ge"}, {"ゴ", "go"},
            {"サ", "sa"}, {"シ", "shi"}, {"ス", "su"}, {"セ", "se"}, {"ソ", "so"},
            {"ザ", "za"}, {"ジ", "ji"}, {"ズ", "zu"}, {"ゼ", "ze"}, {"ゾ", "zo"},
            {"タ", "ta"}, {"チ", "chi"}, {"ツ", "tsu"}, {"テ", "te"}, {"ト", "to"},
            {"ダ", "da"}, {"ヂ", "ji"}, {"ヅ", "zu"}, {"デ", "de"}, {"ド", "do"},
            {"ナ", "na"}, {"ニ", "ni"}, {"ヌ", "nu"}, {"ネ", "ne"}, {"ノ", "no"},
            {"ハ", "ha"}, {"ヒ", "hi"}, {"フ", "fu"}, {"ヘ", "he"}, {"ホ", "ho"},
            {"バ", "ba"}, {"ビ", "bi"}, {"ブ", "bu"}, {"ベ", "be"}, {"ボ", "bo"},
            {"パ", "pa"}, {"ピ", "pi"}, {"プ", "pu"}, {"ペ", "pe"}, {"ポ", "po"},
            {"マ", "ma"}, {"ミ", "mi"}, {"ム", "mu"}, {"メ", "me"}, {"モ", "mo"},
            {"ヤ", "ya"}, {"ユ", "yu"}, {"ヨ", "yo"},
            {"ラ", "ra"}, {"リ", "ri"}, {"ル", "ru"}, {"レ", "re"}, {"ロ", "ro"},
            {"ワ", "wa"}, {"ヲ", "wo"}, {"ン", "n"},
            
            // Small kana
            {"ッ", "q"}, // Small tsu (geminate)
            {"ャ", "ya"}, {"ュ", "yu"}, {"ョ", "yo"},
            {"ァ", "a"}, {"ィ", "i"}, {"ゥ", "u"}, {"ェ", "e"}, {"ォ", "o"},
            
            // Combinations
            {"キャ", "kya"}, {"キュ", "kyu"}, {"キョ", "kyo"},
            {"シャ", "sha"}, {"シュ", "shu"}, {"ショ", "sho"},
            {"チャ", "cha"}, {"チュ", "chu"}, {"チョ", "cho"},
            {"ニャ", "nya"}, {"ニュ", "nyu"}, {"ニョ", "nyo"},
            {"ヒャ", "hya"}, {"ヒュ", "hyu"}, {"ヒョ", "hyo"},
            {"ミャ", "mya"}, {"ミュ", "myu"}, {"ミョ", "myo"},
            {"リャ", "rya"}, {"リュ", "ryu"}, {"リョ", "ryo"},
            {"ギャ", "gya"}, {"ギュ", "gyu"}, {"ギョ", "gyo"},
            {"ジャ", "ja"}, {"ジュ", "ju"}, {"ジョ", "jo"},
            {"ビャ", "bya"}, {"ビュ", "byu"}, {"ビョ", "byo"},
            {"ピャ", "pya"}, {"ピュ", "pyu"}, {"ピョ", "pyo"},
        };
        
        // Apply mappings
        for (const auto& [katakana, phoneme] : mappings) {
            phonemes = StringUtils::Replace(phonemes, katakana, phoneme + " ");
        }
        
        // Clean up multiple spaces
        phonemes = std::regex_replace(phonemes, std::regex(" +"), " ");
        phonemes = StringUtils::Trim(phonemes);
        
        return phonemes;
    }

    std::string RomanizeText(const std::string& text) {
        // Simple romanization fallback
        // This should be more sophisticated in production
        
        std::string romanized = text;
        
        // Basic Hiragana mappings
        std::vector<std::pair<std::string, std::string>> hiragana = {
            {"あ", "a"}, {"い", "i"}, {"う", "u"}, {"え", "e"}, {"お", "o"},
            {"か", "ka"}, {"き", "ki"}, {"く", "ku"}, {"け", "ke"}, {"こ", "ko"},
            {"が", "ga"}, {"ぎ", "gi"}, {"ぐ", "gu"}, {"げ", "ge"}, {"ご", "go"},
            {"さ", "sa"}, {"し", "shi"}, {"す", "su"}, {"せ", "se"}, {"そ", "so"},
            {"ざ", "za"}, {"じ", "ji"}, {"ず", "zu"}, {"ぜ", "ze"}, {"ぞ", "zo"},
            {"た", "ta"}, {"ち", "chi"}, {"つ", "tsu"}, {"て", "te"}, {"と", "to"},
            {"だ", "da"}, {"ぢ", "ji"}, {"づ", "zu"}, {"で", "de"}, {"ど", "do"},
            {"な", "na"}, {"に", "ni"}, {"ぬ", "nu"}, {"ね", "ne"}, {"の", "no"},
            {"は", "ha"}, {"ひ", "hi"}, {"ふ", "fu"}, {"へ", "he"}, {"ほ", "ho"},
            {"ば", "ba"}, {"び", "bi"}, {"ぶ", "bu"}, {"べ", "be"}, {"ぼ", "bo"},
            {"ぱ", "pa"}, {"ぴ", "pi"}, {"ぷ", "pu"}, {"ぺ", "pe"}, {"ぽ", "po"},
            {"ま", "ma"}, {"み", "mi"}, {"む", "mu"}, {"め", "me"}, {"も", "mo"},
            {"や", "ya"}, {"ゆ", "yu"}, {"よ", "yo"},
            {"ら", "ra"}, {"り", "ri"}, {"る", "ru"}, {"れ", "re"}, {"ろ", "ro"},
            {"わ", "wa"}, {"を", "wo"}, {"ん", "n"},
        };
        
        // Apply Hiragana mappings
        for (const auto& [kana, romaji] : hiragana) {
            romanized = StringUtils::Replace(romanized, kana, romaji + " ");
        }
        
        // Apply Katakana conversion (reuse the method)
        romanized = ConvertReadingToPhonemes(romanized);
        
        // Clean up
        romanized = std::regex_replace(romanized, std::regex(" +"), " ");
        romanized = StringUtils::Trim(romanized);
        
        return romanized;
    }

    std::string PostProcessPhonemes(const std::string& phonemes) {
        std::string processed = phonemes;
        
        // Remove duplicate spaces
        processed = std::regex_replace(processed, std::regex(" +"), " ");
        
        // Trim whitespace
        processed = StringUtils::Trim(processed);
        
        // Handle special cases
        // - Double consonants (sokuon)
        processed = std::regex_replace(processed, std::regex("q ([kstph])"), "$1$1");
        
        // - Ensure proper spacing
        if (!processed.empty() && processed[0] == ' ') {
            processed = processed.substr(1);
        }
        if (!processed.empty() && processed[processed.length()-1] == ' ') {
            processed = processed.substr(0, processed.length()-1);
        }
        
        return processed;
    }
};

// ==========================================
// Public Interface Implementation
// ==========================================

JapanesePhonemizer::JapanesePhonemizer() : pImpl(std::make_unique<Impl>()) {}
JapanesePhonemizer::~JapanesePhonemizer() = default;
JapanesePhonemizer::JapanesePhonemizer(JapanesePhonemizer&&) noexcept = default;
JapanesePhonemizer& JapanesePhonemizer::operator=(JapanesePhonemizer&&) noexcept = default;

Status JapanesePhonemizer::Initialize(
    const std::string& mecab_config,
    const std::string& dictionary_path,
    const std::string& onnx_model_path) {
    
    // Initialize MeCab
    Status status = pImpl->mecab.Initialize(mecab_config);
    if (status != Status::OK) {
        std::cerr << "Warning: MeCab initialization failed, using fallback" << std::endl;
        // Continue without MeCab
    }
    
    // Load dictionary if provided
    if (!dictionary_path.empty()) {
        status = pImpl->dictionary.LoadDictionary(dictionary_path);
        if (status != Status::OK) {
            std::cerr << "Warning: Dictionary loading failed" << std::endl;
            pImpl->use_dictionary = false;
        }
    } else {
        pImpl->use_dictionary = false;
    }
    
    // Initialize ONNX model if provided
    if (!onnx_model_path.empty()) {
        status = pImpl->onnx_phonemizer.Initialize(onnx_model_path);
        if (status != Status::OK) {
            std::cerr << "Warning: ONNX phonemizer initialization failed" << std::endl;
            pImpl->use_onnx_fallback = false;
        }
    } else {
        pImpl->use_onnx_fallback = false;
    }
    
    // Mark as initialized if at least one method is available
    pImpl->is_initialized = pImpl->mecab.IsInitialized() ||
                           pImpl->use_dictionary ||
                           pImpl->use_onnx_fallback;
    
    if (!pImpl->is_initialized) {
        std::cerr << "Error: No phonemization method available" << std::endl;
        return Status::ERROR_INITIALIZATION_FAILED;
    }
    
    return Status::OK;
}

std::string JapanesePhonemizer::TextToPhonemes(const std::string& text) {
    if (!pImpl->is_initialized || text.empty()) {
        return "";
    }
    
    std::vector<std::string> phoneme_parts;
    
    // Use MeCab to segment text if available
    if (pImpl->mecab.IsInitialized()) {
        auto morphemes = pImpl->mecab.Parse(text);
        
        for (const auto& morpheme : morphemes) {
            std::string phonemes = pImpl->ProcessMorpheme(morpheme);
            if (!phonemes.empty()) {
                phoneme_parts.push_back(phonemes);
            }
        }
    } else {
        // No MeCab, process as single unit
        MorphemeInfo single_morpheme;
        single_morpheme.surface = text;
        
        std::string phonemes = pImpl->ProcessMorpheme(single_morpheme);
        if (!phonemes.empty()) {
            phoneme_parts.push_back(phonemes);
        }
    }
    
    // Join all phoneme parts
    std::string result = StringUtils::Join(phoneme_parts, " ");
    
    // Post-process phonemes
    result = pImpl->PostProcessPhonemes(result);
    
    return result;
}

std::vector<std::string> JapanesePhonemizer::BatchTextToPhonemes(
    const std::vector<std::string>& texts) {
    
    std::vector<std::string> results;
    results.reserve(texts.size());
    
    for (const auto& text : texts) {
        results.push_back(TextToPhonemes(text));
    }
    
    return results;
}

std::string JapanesePhonemizer::TextToPhonemesWithFallback(
    const std::string& text,
    const std::string& fallback_phonemes) {
    
    if (!fallback_phonemes.empty()) {
        // Use provided fallback phonemes
        return fallback_phonemes;
    }
    
    // Otherwise, generate phonemes normally
    return TextToPhonemes(text);
}

void JapanesePhonemizer::SetUseDictionary(bool use) {
    pImpl->use_dictionary = use && pImpl->dictionary.GetDictionarySize() > 0;
}

void JapanesePhonemizer::SetUseONNXFallback(bool use) {
    pImpl->use_onnx_fallback = use && pImpl->onnx_phonemizer.IsInitialized();
}

bool JapanesePhonemizer::IsInitialized() const {
    return pImpl->is_initialized;
}

PhonemizeStats JapanesePhonemizer::GetStats() const {
    PhonemizeStats stats;
    stats.dictionary_hits = pImpl->dictionary_hits;
    stats.onnx_fallbacks = pImpl->onnx_fallbacks;
    stats.total_words = pImpl->total_words;
    
    if (stats.total_words > 0) {
        stats.dictionary_hit_rate = static_cast<float>(stats.dictionary_hits) / stats.total_words;
        stats.onnx_fallback_rate = static_cast<float>(stats.onnx_fallbacks) / stats.total_words;
    }
    
    return stats;
}

void JapanesePhonemizer::ResetStats() {
    pImpl->dictionary_hits = 0;
    pImpl->onnx_fallbacks = 0;
    pImpl->total_words = 0;
}

MeCabWrapper& JapanesePhonemizer::GetMeCab() {
    return pImpl->mecab;
}

DictionaryLookup& JapanesePhonemizer::GetDictionary() {
    return pImpl->dictionary;
}

PhonemizeONNX& JapanesePhonemizer::GetONNXPhonemizer() {
    return pImpl->onnx_phonemizer;
}

} // namespace jp_edge_tts