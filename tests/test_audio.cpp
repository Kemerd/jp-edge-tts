#include <gtest/gtest.h>
#include "jp_edge_tts/audio/audio_processor.h"
#include "jp_edge_tts/types.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <memory>

using namespace jp_edge_tts;

class AudioTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create audio processor with standard sample rate
        processor = std::make_unique<AudioProcessor>(24000);

        // Create test audio data - a simple sine wave
        CreateTestAudioData();
    }

    void TearDown() override {
        processor.reset();
    }

    void CreateTestAudioData() {
        // Generate 1 second of 440Hz sine wave at 24kHz
        const int sample_rate = 24000;
        const float frequency = 440.0f;
        const float duration = 1.0f;
        const int num_samples = static_cast<int>(duration * sample_rate);

        test_audio.clear();
        test_audio.reserve(num_samples);

        for (int i = 0; i < num_samples; ++i) {
            float t = static_cast<float>(i) / sample_rate;
            float sample = 0.5f * std::sin(2.0f * M_PI * frequency * t);
            test_audio.push_back(sample);
        }

        // Create silent audio for some tests
        silent_audio.resize(1000, 0.0f);

        // Create clipped audio for normalization tests
        clipped_audio = {-1.5f, -1.0f, -0.5f, 0.0f, 0.5f, 1.0f, 1.5f, 2.0f};

        // Create noisy audio for trimming tests
        noisy_audio.resize(1000);
        std::fill(noisy_audio.begin(), noisy_audio.begin() + 100, 0.0f);  // Silent start
        std::fill(noisy_audio.begin() + 100, noisy_audio.begin() + 900, 0.5f);  // Signal
        std::fill(noisy_audio.begin() + 900, noisy_audio.end(), 0.0f);  // Silent end
    }

    std::unique_ptr<AudioProcessor> processor;
    std::vector<float> test_audio;
    std::vector<float> silent_audio;
    std::vector<float> clipped_audio;
    std::vector<float> noisy_audio;
};

TEST_F(AudioTest, ProcessorInitialization) {
    // Test that processor can be created with different sample rates
    auto proc_16k = std::make_unique<AudioProcessor>(16000);
    EXPECT_NE(proc_16k, nullptr);

    auto proc_44k = std::make_unique<AudioProcessor>(44100);
    EXPECT_NE(proc_44k, nullptr);

    auto proc_48k = std::make_unique<AudioProcessor>(48000);
    EXPECT_NE(proc_48k, nullptr);
}

TEST_F(AudioTest, BasicAudioProcessing) {
    // Test basic audio processing with default parameters
    auto processed = processor->ProcessAudio(test_audio);
    EXPECT_EQ(processed.size(), test_audio.size());
    EXPECT_FALSE(processed.empty());

    // Test with custom volume
    auto processed_quiet = processor->ProcessAudio(test_audio, 0.5f);
    EXPECT_EQ(processed_quiet.size(), test_audio.size());

    // Test with normalization disabled
    auto processed_no_norm = processor->ProcessAudio(test_audio, 1.0f, false);
    EXPECT_EQ(processed_no_norm.size(), test_audio.size());
}

TEST_F(AudioTest, VolumeAdjustment) {
    // Test volume adjustment
    auto half_volume = processor->ApplyVolume(test_audio, 0.5f);
    EXPECT_EQ(half_volume.size(), test_audio.size());

    // Check that volume was actually reduced
    for (size_t i = 0; i < std::min(test_audio.size(), half_volume.size()); ++i) {
        EXPECT_FLOAT_EQ(half_volume[i], test_audio[i] * 0.5f);
    }

    // Test zero volume
    auto muted = processor->ApplyVolume(test_audio, 0.0f);
    for (float sample : muted) {
        EXPECT_FLOAT_EQ(sample, 0.0f);
    }

    // Test volume > 1.0
    auto amplified = processor->ApplyVolume(test_audio, 2.0f);
    EXPECT_EQ(amplified.size(), test_audio.size());
}

TEST_F(AudioTest, Normalization) {
    // Test normalization with clipped audio
    auto normalized = processor->Normalize(clipped_audio);
    EXPECT_EQ(normalized.size(), clipped_audio.size());

    // Find max absolute value in normalized audio
    float max_val = 0.0f;
    for (float sample : normalized) {
        max_val = std::max(max_val, std::abs(sample));
    }
    EXPECT_LE(max_val, 1.0f);  // Should be <= 1.0 after normalization

    // Test normalization of silent audio
    auto normalized_silent = processor->Normalize(silent_audio);
    EXPECT_EQ(normalized_silent.size(), silent_audio.size());
    for (float sample : normalized_silent) {
        EXPECT_FLOAT_EQ(sample, 0.0f);
    }
}

TEST_F(AudioTest, SilenceTrimming) {
    // Test silence trimming
    auto trimmed = processor->TrimSilence(noisy_audio, 0.01f);
    EXPECT_LT(trimmed.size(), noisy_audio.size());  // Should be shorter
    EXPECT_FALSE(trimmed.empty());

    // Test with audio that's all silence
    auto trimmed_silent = processor->TrimSilence(silent_audio, 0.01f);
    // May be empty or very short after trimming silence

    // Test with very low threshold (should trim very little)
    auto trimmed_low_thresh = processor->TrimSilence(noisy_audio, 0.001f);
    EXPECT_GE(trimmed_low_thresh.size(), trimmed.size());
}

TEST_F(AudioTest, FadeApplication) {
    // Test fade application
    auto faded = processor->ApplyFade(test_audio, 100);  // 100ms fade
    EXPECT_EQ(faded.size(), test_audio.size());

    // Check that beginning and end are quieter (fade effect)
    const int fade_samples = (100 * 24000) / 1000;  // 100ms at 24kHz
    if (faded.size() > fade_samples * 2) {
        // Check fade-in: early samples should be quieter
        EXPECT_LT(std::abs(faded[0]), std::abs(test_audio[0]));

        // Check fade-out: late samples should be quieter
        size_t end_idx = faded.size() - 1;
        EXPECT_LT(std::abs(faded[end_idx]), std::abs(test_audio[end_idx]));
    }

    // Test with zero fade duration
    auto no_fade = processor->ApplyFade(test_audio, 0);
    EXPECT_EQ(no_fade.size(), test_audio.size());
}

TEST_F(AudioTest, LevelMeasurement) {
    // Test RMS calculation
    float rms = processor->GetRMS(test_audio);
    EXPECT_GT(rms, 0.0f);
    EXPECT_LT(rms, 1.0f);

    // Test RMS of silence
    float rms_silent = processor->GetRMS(silent_audio);
    EXPECT_FLOAT_EQ(rms_silent, 0.0f);

    // Test peak level
    float peak = processor->GetPeakLevel(test_audio);
    EXPECT_GT(peak, 0.0f);
    EXPECT_LE(peak, 1.0f);

    // Test peak of silence
    float peak_silent = processor->GetPeakLevel(silent_audio);
    EXPECT_FLOAT_EQ(peak_silent, 0.0f);

    // Test peak of clipped audio
    float peak_clipped = processor->GetPeakLevel(clipped_audio);
    EXPECT_GT(peak_clipped, 1.0f);  // Should detect clipping
}

TEST_F(AudioTest, FormatConversion) {
    // Test float to PCM16 conversion
    auto pcm16 = processor->ToPCM16(test_audio);
    EXPECT_EQ(pcm16.size(), test_audio.size());

    // Test PCM16 to float conversion
    auto float_samples = processor->FromPCM16(pcm16);
    EXPECT_EQ(float_samples.size(), pcm16.size());

    // Test round-trip conversion (with some tolerance for quantization)
    EXPECT_EQ(float_samples.size(), test_audio.size());
    for (size_t i = 0; i < std::min(test_audio.size(), float_samples.size()); ++i) {
        EXPECT_NEAR(float_samples[i], test_audio[i], 1.0f / 32768.0f);  // 16-bit precision
    }

    // Test conversion of extreme values
    std::vector<float> extreme = {-1.0f, 0.0f, 1.0f};
    auto extreme_pcm = processor->ToPCM16(extreme);
    EXPECT_EQ(extreme_pcm.size(), 3);
    EXPECT_EQ(extreme_pcm[0], -32768);  // -1.0f -> min PCM16
    EXPECT_EQ(extreme_pcm[1], 0);       // 0.0f -> 0
    EXPECT_EQ(extreme_pcm[2], 32767);   // 1.0f -> max PCM16
}

TEST_F(AudioTest, ResamplingOperations) {
    // Test resampling from 24kHz to 16kHz
    auto resampled_down = processor->Resample(test_audio, 24000, 16000);
    size_t expected_size = (test_audio.size() * 16000) / 24000;
    EXPECT_NEAR(resampled_down.size(), expected_size, expected_size * 0.1);  // Within 10%

    // Test resampling from 24kHz to 44.1kHz
    auto resampled_up = processor->Resample(test_audio, 24000, 44100);
    expected_size = (test_audio.size() * 44100) / 24000;
    EXPECT_NEAR(resampled_up.size(), expected_size, expected_size * 0.1);  // Within 10%

    // Test same sample rate (should be nearly identical)
    auto resampled_same = processor->Resample(test_audio, 24000, 24000);
    EXPECT_EQ(resampled_same.size(), test_audio.size());
}

TEST_F(AudioTest, PitchAndSpeedModification) {
    // Test pitch shifting
    auto pitch_up = processor->ApplyPitchShift(test_audio, 1.5f);
    EXPECT_EQ(pitch_up.size(), test_audio.size());  // Length should be preserved

    auto pitch_down = processor->ApplyPitchShift(test_audio, 0.75f);
    EXPECT_EQ(pitch_down.size(), test_audio.size());

    // Test speed change
    auto speed_up = processor->ApplySpeedChange(test_audio, 1.5f);
    EXPECT_LT(speed_up.size(), test_audio.size());  // Should be shorter

    auto speed_down = processor->ApplySpeedChange(test_audio, 0.75f);
    EXPECT_GT(speed_down.size(), test_audio.size());  // Should be longer
}

TEST_F(AudioTest, EdgeCases) {
    // Test empty audio
    std::vector<float> empty;
    auto processed_empty = processor->ProcessAudio(empty);
    EXPECT_TRUE(processed_empty.empty());

    auto normalized_empty = processor->Normalize(empty);
    EXPECT_TRUE(normalized_empty.empty());

    auto volume_empty = processor->ApplyVolume(empty, 0.5f);
    EXPECT_TRUE(volume_empty.empty());

    // Test single sample
    std::vector<float> single = {0.5f};
    auto processed_single = processor->ProcessAudio(single);
    EXPECT_EQ(processed_single.size(), 1);

    // Test with NaN and infinity (should be handled gracefully)
    std::vector<float> invalid = {std::numeric_limits<float>::quiet_NaN(),
                                  std::numeric_limits<float>::infinity(),
                                  -std::numeric_limits<float>::infinity()};
    auto processed_invalid = processor->ProcessAudio(invalid);
    EXPECT_EQ(processed_invalid.size(), invalid.size());

    // Processed audio should not contain NaN or infinity
    for (float sample : processed_invalid) {
        EXPECT_FALSE(std::isnan(sample));
        EXPECT_FALSE(std::isinf(sample));
    }
}

TEST_F(AudioTest, WavConversion) {
    // Create test audio data structure
    AudioData audio_data;
    audio_data.samples = test_audio;
    audio_data.sample_rate = 24000;
    audio_data.channels = 1;

    // Test conversion to WAV bytes
    auto wav_bytes = processor->ToWavBytes(audio_data, AudioFormat::WAV_PCM16);
    EXPECT_FALSE(wav_bytes.empty());
    EXPECT_GT(wav_bytes.size(), 44);  // Should have at least WAV header (44 bytes)

    // Test different formats
    auto wav_float = processor->ToWavBytes(audio_data, AudioFormat::WAV_FLOAT32);
    EXPECT_FALSE(wav_float.empty());
    EXPECT_NE(wav_bytes.size(), wav_float.size());  // Should be different sizes
}

TEST_F(AudioTest, ErrorHandling) {
    // Test invalid parameters
    auto invalid_volume = processor->ApplyVolume(test_audio, -1.0f);
    EXPECT_EQ(invalid_volume.size(), test_audio.size());  // Should handle gracefully

    // Test invalid resample rates
    auto invalid_resample = processor->Resample(test_audio, 0, 44100);
    // Should handle gracefully (may return empty or original)

    // Test invalid pitch/speed factors
    auto invalid_pitch = processor->ApplyPitchShift(test_audio, 0.0f);
    EXPECT_EQ(invalid_pitch.size(), test_audio.size());

    auto invalid_speed = processor->ApplySpeedChange(test_audio, 0.0f);
    // Should handle gracefully
}