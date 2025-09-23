#include <gtest/gtest.h>
#include "jp_edge_tts/tokenizer/ipa_tokenizer.h"
#include <string>
#include <vector>
#include <memory>

using namespace jp_edge_tts;

class TokenizerTest : public ::testing::Test {
protected:
    void SetUp() override {
        tokenizer = std::make_unique<IPATokenizer>();

        // Create a minimal test vocabulary in JSON format
        test_vocab_json = R"({
            "<pad>": 0,
            "<unk>": 1,
            "<start>": 2,
            "<end>": 3,
            "a": 4,
            "i": 5,
            "u": 6,
            "e": 7,
            "o": 8,
            "k": 9,
            "s": 10,
            "t": 11,
            "n": 12,
            "m": 13,
            "r": 14,
            "w": 15,
            "h": 16,
            "j": 17,
            "ɲ": 18,
            "ʃ": 19,
            "tʃ": 20,
            "dʒ": 21
        })";
    }

    void TearDown() override {
        tokenizer.reset();
    }

    std::unique_ptr<IPATokenizer> tokenizer;
    std::string test_vocab_json;
};

TEST_F(TokenizerTest, InitialState) {
    // Test initial state before vocabulary loading
    EXPECT_FALSE(tokenizer->IsLoaded());
    EXPECT_EQ(tokenizer->GetVocabularySize(), 0);

    // Should handle empty phonemes gracefully
    auto empty_tokens = tokenizer->PhonemesToTokens("");
    EXPECT_TRUE(empty_tokens.empty());
}

TEST_F(TokenizerTest, VocabularyLoading) {
    // Test loading vocabulary from JSON string
    bool loaded = tokenizer->LoadVocabularyFromJSON(test_vocab_json);
    EXPECT_TRUE(loaded);
    EXPECT_TRUE(tokenizer->IsLoaded());
    EXPECT_GT(tokenizer->GetVocabularySize(), 0);

    // Test invalid JSON
    auto tokenizer2 = std::make_unique<IPATokenizer>();
    bool loaded_invalid = tokenizer2->LoadVocabularyFromJSON("invalid json");
    EXPECT_FALSE(loaded_invalid);
    EXPECT_FALSE(tokenizer2->IsLoaded());
}

TEST_F(TokenizerTest, BasicTokenConversion) {
    // Load vocabulary first
    ASSERT_TRUE(tokenizer->LoadVocabularyFromJSON(test_vocab_json));

    // Test individual phoneme to token conversion
    int token_a = tokenizer->GetTokenId("a");
    EXPECT_EQ(token_a, 4);

    int token_k = tokenizer->GetTokenId("k");
    EXPECT_EQ(token_k, 9);

    // Test unknown phoneme
    int token_unknown = tokenizer->GetTokenId("xyz_unknown");
    EXPECT_EQ(token_unknown, 1);  // Should return unknown token ID
}

TEST_F(TokenizerTest, PhonemeStringTokenization) {
    // Load vocabulary first
    ASSERT_TRUE(tokenizer->LoadVocabularyFromJSON(test_vocab_json));

    // Test space-separated phonemes
    std::string phonemes = "a k i";
    auto tokens = tokenizer->PhonemesToTokens(phonemes);
    std::vector<int> expected = {4, 9, 5};  // a=4, k=9, i=5
    EXPECT_EQ(tokens, expected);

    // Test empty string
    auto empty_tokens = tokenizer->PhonemesToTokens("");
    EXPECT_TRUE(empty_tokens.empty());

    // Test single phoneme
    auto single_tokens = tokenizer->PhonemesToTokens("a");
    EXPECT_EQ(single_tokens.size(), 1);
    EXPECT_EQ(single_tokens[0], 4);
}

TEST_F(TokenizerTest, PhonemeListTokenization) {
    // Load vocabulary first
    ASSERT_TRUE(tokenizer->LoadVocabularyFromJSON(test_vocab_json));

    // Test vector of phonemes
    std::vector<std::string> phoneme_list = {"a", "k", "i"};
    auto tokens = tokenizer->PhonemesToTokens(phoneme_list);
    std::vector<int> expected = {4, 9, 5};
    EXPECT_EQ(tokens, expected);

    // Test empty vector
    std::vector<std::string> empty_list;
    auto empty_tokens = tokenizer->PhonemesToTokens(empty_list);
    EXPECT_TRUE(empty_tokens.empty());
}

TEST_F(TokenizerTest, TokenToPhonemeConversion) {
    // Load vocabulary first
    ASSERT_TRUE(tokenizer->LoadVocabularyFromJSON(test_vocab_json));

    // Test token to phoneme conversion
    std::string phoneme_a = tokenizer->GetPhoneme(4);
    EXPECT_EQ(phoneme_a, "a");

    std::string phoneme_k = tokenizer->GetPhoneme(9);
    EXPECT_EQ(phoneme_k, "k");

    // Test invalid token ID
    std::string invalid_phoneme = tokenizer->GetPhoneme(9999);
    EXPECT_TRUE(invalid_phoneme.empty());
}

TEST_F(TokenizerTest, TokenSequenceToPhonemes) {
    // Load vocabulary first
    ASSERT_TRUE(tokenizer->LoadVocabularyFromJSON(test_vocab_json));

    // Test converting token sequence back to phonemes
    std::vector<int> tokens = {4, 9, 5};  // a, k, i
    std::string phonemes = tokenizer->TokensToPhonemes(tokens);
    EXPECT_EQ(phonemes, "a k i");

    // Test empty token sequence
    std::vector<int> empty_tokens;
    std::string empty_phonemes = tokenizer->TokensToPhonemes(empty_tokens);
    EXPECT_EQ(empty_phonemes, "");

    // Test single token
    std::vector<int> single_token = {4};
    std::string single_phoneme = tokenizer->TokensToPhonemes(single_token);
    EXPECT_EQ(single_phoneme, "a");
}

TEST_F(TokenizerTest, SpecialTokens) {
    // Load vocabulary first
    ASSERT_TRUE(tokenizer->LoadVocabularyFromJSON(test_vocab_json));

    // Test special tokens
    auto special = tokenizer->GetSpecialTokens();
    EXPECT_EQ(special.pad_token, 0);
    EXPECT_EQ(special.unk_token, 1);

    // Test that special tokens exist in vocabulary
    EXPECT_EQ(tokenizer->GetPhoneme(special.pad_token), "<pad>");
    EXPECT_EQ(tokenizer->GetPhoneme(special.unk_token), "<unk>");
}

TEST_F(TokenizerTest, PhonemeInventory) {
    // Load vocabulary first
    ASSERT_TRUE(tokenizer->LoadVocabularyFromJSON(test_vocab_json));

    // Test getting all phonemes
    auto phonemes = tokenizer->GetPhonemes();
    EXPECT_FALSE(phonemes.empty());

    // Check that basic phonemes are included
    bool has_a = false, has_k = false;
    for (const auto& phoneme : phonemes) {
        if (phoneme == "a") has_a = true;
        if (phoneme == "k") has_k = true;
    }
    EXPECT_TRUE(has_a);
    EXPECT_TRUE(has_k);
}

TEST_F(TokenizerTest, PaddingOperations) {
    // Load vocabulary first
    ASSERT_TRUE(tokenizer->LoadVocabularyFromJSON(test_vocab_json));

    // Test padding to target length
    std::vector<int> tokens = {4, 9, 5};  // Length 3
    auto padded = tokenizer->PadTokens(tokens, 5, false);  // Pad right
    EXPECT_EQ(padded.size(), 5);
    EXPECT_EQ(padded[0], 4);  // Original tokens preserved
    EXPECT_EQ(padded[1], 9);
    EXPECT_EQ(padded[2], 5);
    EXPECT_EQ(padded[3], 0);  // Pad token
    EXPECT_EQ(padded[4], 0);  // Pad token

    // Test left padding
    auto left_padded = tokenizer->PadTokens(tokens, 5, true);
    EXPECT_EQ(left_padded.size(), 5);
    EXPECT_EQ(left_padded[0], 0);  // Pad token
    EXPECT_EQ(left_padded[1], 0);  // Pad token
    EXPECT_EQ(left_padded[2], 4);  // Original tokens
    EXPECT_EQ(left_padded[3], 9);
    EXPECT_EQ(left_padded[4], 5);

    // Test no padding needed
    auto no_pad = tokenizer->PadTokens(tokens, 3, false);
    EXPECT_EQ(no_pad, tokens);
}

TEST_F(TokenizerTest, TruncationOperations) {
    // Load vocabulary first
    ASSERT_TRUE(tokenizer->LoadVocabularyFromJSON(test_vocab_json));

    // Test truncation
    std::vector<int> tokens = {4, 9, 5, 7, 8};  // Length 5
    auto truncated = tokenizer->TruncateTokens(tokens, 3);
    EXPECT_EQ(truncated.size(), 3);
    EXPECT_EQ(truncated[0], 4);
    EXPECT_EQ(truncated[1], 9);
    EXPECT_EQ(truncated[2], 5);

    // Test no truncation needed
    auto no_trunc = tokenizer->TruncateTokens(tokens, 10);
    EXPECT_EQ(no_trunc, tokens);

    // Test truncation to zero
    auto zero_trunc = tokenizer->TruncateTokens(tokens, 0);
    EXPECT_TRUE(zero_trunc.empty());
}

TEST_F(TokenizerTest, RoundTripConversion) {
    // Load vocabulary first
    ASSERT_TRUE(tokenizer->LoadVocabularyFromJSON(test_vocab_json));

    // Test round-trip conversion: phonemes -> tokens -> phonemes
    std::string original = "a k i u e";
    auto tokens = tokenizer->PhonemesToTokens(original);
    std::string recovered = tokenizer->TokensToPhonemes(tokens);
    EXPECT_EQ(original, recovered);
}

TEST_F(TokenizerTest, UnknownPhonemeHandling) {
    // Load vocabulary first
    ASSERT_TRUE(tokenizer->LoadVocabularyFromJSON(test_vocab_json));

    // Test handling of unknown phonemes
    std::string with_unknown = "a xyz k";
    auto tokens = tokenizer->PhonemesToTokens(with_unknown);
    std::vector<int> expected = {4, 1, 9};  // a, <unk>, k
    EXPECT_EQ(tokens, expected);

    // Convert back - unknown should become <unk>
    std::string recovered = tokenizer->TokensToPhonemes(tokens);
    EXPECT_EQ(recovered, "a <unk> k");
}