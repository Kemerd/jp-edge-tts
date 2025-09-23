/**
 * @file mecab_wrapper.cpp
 * @brief Implementation of MeCab wrapper for Japanese tokenization
 * D Everett Hinton
 * @date 2025
 *
 * @copyright MIT License
 */

#include "jp_edge_tts/tokenizer/mecab_wrapper.h"

#ifdef USE_MECAB
#include <mecab.h>
#endif

#include <sstream>
#include <algorithm>
#include <codecvt>
#include <locale>
#include <regex>

namespace jp_edge_tts {

// ==========================================
// Private Implementation
// ==========================================

class MeCabWrapper::Impl {
public:
#ifdef USE_MECAB
    MeCab::Tagger* tagger = nullptr;
    MeCab::Model* model = nullptr;
#endif
    Config config;
    bool initialized = false;

    Impl(const Config& cfg) : config(cfg) {}

    ~Impl() {
#ifdef USE_MECAB
        if (tagger) {
            delete tagger;
            tagger = nullptr;
        }
        if (model) {
            delete model;
            model = nullptr;
        }
#endif
    }

    bool Initialize() {
#ifdef USE_MECAB
        try {
            // Build MeCab arguments
            std::string args = "mecab";

            // Add dictionary directory if specified
            if (!config.dic_dir.empty()) {
                args += " -d " + config.dic_dir;
            }

            // Add user dictionary if specified
            if (!config.user_dic.empty()) {
                args += " -u " + config.user_dic;
            }

            // Create model and tagger
            model = MeCab::createModel(args.c_str());
            if (!model) {
                return false;
            }

            tagger = model->createTagger();
            if (!tagger) {
                delete model;
                model = nullptr;
                return false;
            }

            initialized = true;
            return true;

        } catch (const std::exception& e) {
            // Clean up on failure
            if (tagger) {
                delete tagger;
                tagger = nullptr;
            }
            if (model) {
                delete model;
                model = nullptr;
            }
            return false;
        }
#else
        // MeCab not available, use fallback
        initialized = true;
        return true;
#endif
    }

    std::vector<MorphemeInfo> Parse(const std::string& text) {
        std::vector<MorphemeInfo> result;

#ifdef USE_MECAB
        if (!initialized || !tagger) {
            return result;
        }

        // Normalize text if configured
        std::string input = config.normalize ?
                           MeCabWrapper::NormalizeText(text) : text;

        // Parse with MeCab
        const MeCab::Node* node = tagger->parseToNode(input.c_str());

        while (node) {
            // Skip BOS/EOS nodes
            if (node->stat != MECAB_BOS_NODE && node->stat != MECAB_EOS_NODE) {
                MorphemeInfo info;

                // Extract surface form
                info.surface = std::string(node->surface, node->length);

                // Parse features (CSV format)
                std::string features = node->feature;
                std::vector<std::string> feature_list = SplitCSV(features);

                // Standard feature format:
                // 0: Part of speech (major)
                // 1: Part of speech (sub1)
                // 2: Part of speech (sub2)
                // 3: Part of speech (sub3)
                // 4: Inflection type
                // 5: Inflection form
                // 6: Base form
                // 7: Reading (Katakana)
                // 8: Pronunciation

                if (feature_list.size() > 0) {
                    info.pos = feature_list[0];
                }

                if (feature_list.size() > 6 && feature_list[6] != "*") {
                    info.base_form = feature_list[6];
                } else {
                    info.base_form = info.surface;
                }

                if (feature_list.size() > 7 && feature_list[7] != "*") {
                    info.reading = feature_list[7];
                } else {
                    // If no reading, try to generate it
                    info.reading = GenerateReading(info.surface);
                }

                if (feature_list.size() > 8 && feature_list[8] != "*") {
                    info.pronunciation = feature_list[8];
                } else {
                    info.pronunciation = info.reading;
                }

                result.push_back(info);
            }

            node = node->next;
        }
#else
        // Fallback: character-based tokenization
        result = FallbackParse(text);
#endif

        return result;
    }

private:
    /**
     * @brief Split CSV string
     */
    std::vector<std::string> SplitCSV(const std::string& str) {
        std::vector<std::string> result;
        std::stringstream ss(str);
        std::string item;

        while (std::getline(ss, item, ',')) {
            result.push_back(item);
        }

        return result;
    }

    /**
     * @brief Generate reading for text without MeCab
     */
    std::string GenerateReading(const std::string& text) {
        // For pure Hiragana, convert to Katakana
        if (MeCabWrapper::IsPureHiragana(text)) {
            return MeCabWrapper::HiraganaToKatakana(text);
        }

        // For pure Katakana, return as is
        if (MeCabWrapper::IsPureKatakana(text)) {
            return text;
        }

        // For mixed or Kanji, we need dictionary lookup
        // This is a limitation of the fallback method
        return text;
    }

    /**
     * @brief Fallback parsing without MeCab
     */
    std::vector<MorphemeInfo> FallbackParse(const std::string& text) {
        std::vector<MorphemeInfo> result;

        // Simple character-based segmentation
        std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
        std::u32string u32text = converter.from_bytes(text);

        std::u32string current_word;
        bool in_hiragana = false;
        bool in_katakana = false;
        bool in_kanji = false;

        for (char32_t ch : u32text) {
            bool is_hiragana = (ch >= 0x3040 && ch <= 0x309F);
            bool is_katakana = (ch >= 0x30A0 && ch <= 0x30FF);
            bool is_kanji = (ch >= 0x4E00 && ch <= 0x9FAF);
            bool is_punctuation = (ch < 0x80) || (ch >= 0x3000 && ch <= 0x303F);

            // Check for script change
            if ((is_hiragana && !in_hiragana) ||
                (is_katakana && !in_katakana) ||
                (is_kanji && !in_kanji) ||
                is_punctuation) {

                // Save current word if any
                if (!current_word.empty()) {
                    MorphemeInfo info;
                    info.surface = converter.to_bytes(current_word);
                    info.reading = GenerateReading(info.surface);
                    info.pronunciation = info.reading;
                    info.base_form = info.surface;
                    info.pos = GuessPartOfSpeech(current_word);
                    result.push_back(info);
                    current_word.clear();
                }

                if (is_punctuation) {
                    // Add punctuation immediately
                    MorphemeInfo info;
                    info.surface = converter.to_bytes(std::u32string(1, ch));
                    info.reading = info.surface;
                    info.pronunciation = info.surface;
                    info.base_form = info.surface;
                    info.pos = "記号";
                    result.push_back(info);
                }
            }

            if (!is_punctuation) {
                current_word += ch;
            }

            in_hiragana = is_hiragana;
            in_katakana = is_katakana;
            in_kanji = is_kanji;
        }

        // Add final word
        if (!current_word.empty()) {
            MorphemeInfo info;
            info.surface = converter.to_bytes(current_word);
            info.reading = GenerateReading(info.surface);
            info.pronunciation = info.reading;
            info.base_form = info.surface;
            info.pos = GuessPartOfSpeech(current_word);
            result.push_back(info);
        }

        return result;
    }

    /**
     * @brief Guess part of speech from character type
     */
    std::string GuessPartOfSpeech(const std::u32string& word) {
        if (word.empty()) return "Unknown";

        char32_t first = word[0];

        if (first >= 0x3040 && first <= 0x309F) {
            // Hiragana - likely particle or auxiliary verb
            if (word.length() <= 2) {
                return "助詞";  // Particle
            }
            return "動詞";  // Verb
        } else if (first >= 0x30A0 && first <= 0x30FF) {
            // Katakana - likely noun (foreign word)
            return "名詞";
        } else if (first >= 0x4E00 && first <= 0x9FAF) {
            // Kanji - likely noun or verb stem
            return "名詞";
        }

        return "Unknown";
    }
};

// ==========================================
// Public Interface Implementation
// ==========================================

MeCabWrapper::MeCabWrapper(const Config& config)
    : pImpl(std::make_unique<Impl>(config)) {
}

MeCabWrapper::~MeCabWrapper() = default;
MeCabWrapper::MeCabWrapper(MeCabWrapper&&) noexcept = default;
MeCabWrapper& MeCabWrapper::operator=(MeCabWrapper&&) noexcept = default;

bool MeCabWrapper::Initialize() {
    return pImpl->Initialize();
}

bool MeCabWrapper::IsInitialized() const {
    return pImpl->initialized;
}

std::vector<MorphemeInfo> MeCabWrapper::Parse(const std::string& text) {
    return pImpl->Parse(text);
}

std::vector<std::string> MeCabWrapper::Tokenize(const std::string& text) {
    auto morphemes = Parse(text);
    std::vector<std::string> tokens;

    for (const auto& morpheme : morphemes) {
        tokens.push_back(morpheme.surface);
    }

    return tokens;
}

std::string MeCabWrapper::GetReading(const std::string& text) {
    auto morphemes = Parse(text);
    std::stringstream ss;

    for (const auto& morpheme : morphemes) {
        if (!morpheme.reading.empty()) {
            ss << morpheme.reading;
        } else {
            ss << morpheme.surface;
        }
    }

    return ss.str();
}

std::vector<std::string> MeCabWrapper::GetReadings(const std::string& text) {
    auto morphemes = Parse(text);
    std::vector<std::string> readings;

    for (const auto& morpheme : morphemes) {
        if (!morpheme.reading.empty()) {
            readings.push_back(morpheme.reading);
        } else {
            readings.push_back(morpheme.surface);
        }
    }

    return readings;
}

// Static utility functions

std::string MeCabWrapper::KatakanaToHiragana(const std::string& katakana) {
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
    std::u32string u32str = converter.from_bytes(katakana);

    for (auto& ch : u32str) {
        // Convert Katakana (30A0-30FF) to Hiragana (3040-309F)
        if (ch >= 0x30A0 && ch <= 0x30FF) {
            ch -= 0x60;
        }
    }

    return converter.to_bytes(u32str);
}

std::string MeCabWrapper::HiraganaToKatakana(const std::string& hiragana) {
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
    std::u32string u32str = converter.from_bytes(hiragana);

    for (auto& ch : u32str) {
        // Convert Hiragana (3040-309F) to Katakana (30A0-30FF)
        if (ch >= 0x3040 && ch <= 0x309F) {
            ch += 0x60;
        }
    }

    return converter.to_bytes(u32str);
}

std::string MeCabWrapper::NormalizeText(const std::string& text) {
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
    std::u32string u32str = converter.from_bytes(text);

    for (auto& ch : u32str) {
        // Convert full-width ASCII to half-width
        if (ch >= 0xFF01 && ch <= 0xFF5E) {
            ch = ch - 0xFF01 + 0x21;
        }
        // Convert full-width space
        else if (ch == 0x3000) {
            ch = 0x20;
        }
    }

    return converter.to_bytes(u32str);
}

bool MeCabWrapper::ContainsKanji(const std::string& text) {
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
    std::u32string u32str = converter.from_bytes(text);

    for (char32_t ch : u32str) {
        if (ch >= 0x4E00 && ch <= 0x9FAF) {
            return true;
        }
    }

    return false;
}

bool MeCabWrapper::IsPureHiragana(const std::string& text) {
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
    std::u32string u32str = converter.from_bytes(text);

    for (char32_t ch : u32str) {
        // Allow Hiragana and Japanese punctuation
        if (!((ch >= 0x3040 && ch <= 0x309F) ||
              (ch >= 0x3000 && ch <= 0x303F))) {
            return false;
        }
    }

    return true;
}

bool MeCabWrapper::IsPureKatakana(const std::string& text) {
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
    std::u32string u32str = converter.from_bytes(text);

    for (char32_t ch : u32str) {
        // Allow Katakana and Japanese punctuation
        if (!((ch >= 0x30A0 && ch <= 0x30FF) ||
              (ch >= 0x3000 && ch <= 0x303F))) {
            return false;
        }
    }

    return true;
}

std::string MeCabWrapper::GetVersion() const {
#ifdef USE_MECAB
    return MeCab::Model::version();
#else
    return "Fallback tokenizer (no MeCab)";
#endif
}

std::string MeCabWrapper::GetDictionaryInfo() const {
#ifdef USE_MECAB
    if (pImpl->tagger) {
        return pImpl->tagger->dictionary_info()->filename;
    }
#endif
    return "No dictionary";
}

bool MeCabWrapper::AddUserDictionary(const std::string& path) {
    // Would require re-initialization with new dictionary
    pImpl->config.user_dic = path;
    return pImpl->Initialize();
}

// Factory functions

std::unique_ptr<MeCabWrapper> CreateMeCabTokenizer() {
    return std::make_unique<MeCabWrapper>();
}

std::unique_ptr<MeCabWrapper> CreateMeCabTokenizer(const MeCabWrapper::Config& config) {
    return std::make_unique<MeCabWrapper>(config);
}

} // namespace jp_edge_tts