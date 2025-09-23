#include <gtest/gtest.h>
#include "jp_edge_tts/phonemizer/japanese_phonemizer.h"
#include "jp_edge_tts/types.h"
#include <string>
#include <vector>
#include <memory>

using namespace jp_edge_tts;

class PhonemizerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create phonemizer with basic config for testing
        JapanesePhonemizer::Config config;
        config.enable_cache = false;  // Disable cache for deterministic tests
        config.use_mecab = false;     // Use fallback for unit tests
        phonemizer = CreatePhonemizer(config);
    }

    void TearDown() override {
        phonemizer.reset();
    }

    std::unique_ptr<JapanesePhonemizer> phonemizer;
};

TEST_F(PhonemizerTest, FactoryCreation) {
    // Test that factory functions work
    auto default_phonemizer = CreatePhonemizer();
    EXPECT_NE(default_phonemizer, nullptr);

    JapanesePhonemizer::Config custom_config;
    custom_config.enable_cache = true;
    auto custom_phonemizer = CreatePhonemizer(custom_config);
    EXPECT_NE(custom_phonemizer, nullptr);
}

TEST_F(PhonemizerTest, InitializationState) {
    // Test initialization state tracking
    EXPECT_FALSE(phonemizer->IsInitialized());

    // Note: Initialize() might fail without model files, but we test the interface
    Status status = phonemizer->Initialize();
    // Don't assert on status as models might not be available in test environment
}

TEST_F(PhonemizerTest, TextNormalization) {
    // Test text normalization functionality
    std::string normalized = phonemizer->NormalizeText("１２３４５");
    EXPECT_FALSE(normalized.empty());

    // Test empty string
    EXPECT_EQ(phonemizer->NormalizeText(""), "");

    // Test already normalized text
    std::string simple = "hello";
    EXPECT_FALSE(phonemizer->NormalizeText(simple).empty());
}

TEST_F(PhonemizerTest, TextSegmentation) {
    // Test text segmentation
    auto segments = phonemizer->SegmentText("こんにちは");
    EXPECT_FALSE(segments.empty());

    // Test empty string
    auto empty_segments = phonemizer->SegmentText("");
    EXPECT_TRUE(empty_segments.empty());

    // Test single character
    auto single_segments = phonemizer->SegmentText("あ");
    EXPECT_EQ(single_segments.size(), 1);
    EXPECT_EQ(single_segments[0], "あ");
}

TEST_F(PhonemizerTest, DictionaryOperations) {
    // Test dictionary lookup with non-existent word
    auto result = phonemizer->LookupDictionary("nonexistent_word_12345");
    EXPECT_FALSE(result.has_value());

    // Test adding to dictionary
    bool added = phonemizer->AddToDictionary("テスト", "t e s u t o");
    // May succeed or fail depending on implementation state

    // Test removal
    bool removed = phonemizer->RemoveFromDictionary("nonexistent_word");
    EXPECT_FALSE(removed);  // Should be false for non-existent word
}

TEST_F(PhonemizerTest, CacheOperations) {
    // Test cache statistics
    auto stats = phonemizer->GetCacheStats();
    EXPECT_EQ(stats.total_entries, 0);  // Should start empty
    EXPECT_EQ(stats.hit_count, 0);
    EXPECT_EQ(stats.miss_count, 0);

    // Test cache control
    phonemizer->ClearCache();  // Should not crash
    phonemizer->SetMaxCacheSize(1000);

    // Test enabling/disabling cache
    phonemizer->EnableCache(true);
    phonemizer->EnableCache(false);
}

TEST_F(PhonemizerTest, FeatureControls) {
    // Test feature enable/disable
    phonemizer->EnableMeCab(true);
    phonemizer->EnableMeCab(false);

    phonemizer->EnableNormalization(true);
    phonemizer->EnableNormalization(false);
}

TEST_F(PhonemizerTest, PhonemeConversionFunctions) {
    // Test utility conversion functions
    std::string romaji_result = RomajiToPhonemes("konnichiwa");
    EXPECT_FALSE(romaji_result.empty());

    std::string hiragana_result = HiraganaToPhonemes("こんにちは");
    EXPECT_FALSE(hiragana_result.empty());

    std::string katakana_result = KatakanaToPhonemes("コンニチハ");
    EXPECT_FALSE(katakana_result.empty());

    // Test empty inputs
    EXPECT_EQ(RomajiToPhonemes(""), "");
    EXPECT_EQ(HiraganaToPhonemes(""), "");
    EXPECT_EQ(KatakanaToPhonemes(""), "");
}

TEST_F(PhonemizerTest, PhonemeSetRetrieval) {
    // Test phoneme set retrieval
    auto phoneme_set = phonemizer->GetPhonemeSet();
    EXPECT_FALSE(phoneme_set.empty());  // Should have some phonemes defined

    // Check for basic IPA phonemes
    bool has_vowels = false;
    for (const auto& phoneme : phoneme_set) {
        if (phoneme == "a" || phoneme == "i" || phoneme == "u" ||
            phoneme == "e" || phoneme == "o") {
            has_vowels = true;
            break;
        }
    }
    EXPECT_TRUE(has_vowels);
}

TEST_F(PhonemizerTest, ErrorHandling) {
    // Test error handling with invalid inputs
    std::string result = phonemizer->Phonemize("");
    EXPECT_EQ(result, "");  // Empty input should return empty result

    // Test with very long string
    std::string long_text(10000, 'あ');
    std::string long_result = phonemizer->Phonemize(long_text);
    // Should not crash, result depends on implementation
}

TEST_F(PhonemizerTest, BatchProcessing) {
    // Test batch phonemization
    std::vector<std::string> texts = {"こんにちは", "さようなら", ""};
    auto results = phonemizer->PhonemizeBatch(texts);
    EXPECT_EQ(results.size(), texts.size());

    // Empty batch
    auto empty_results = phonemizer->PhonemizeBatch({});
    EXPECT_TRUE(empty_results.empty());
}