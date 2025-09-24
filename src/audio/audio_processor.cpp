/**
 * @file audio_processor.cpp
 * @brief Implementation of audio processing and manipulation
 * @author D Everett Hinton
 * @date 2025
 *
 * @copyright MIT License
 */

#include "jp_edge_tts/audio/audio_processor.h"
#include "jp_edge_tts/audio/wav_writer.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <iostream>
#include <complex>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace jp_edge_tts {

// ==========================================
// Private Implementation
// ==========================================

class AudioProcessor::Impl {
public:
    int sample_rate;

    Impl(int sample_rate) : sample_rate(sample_rate) {}

    std::vector<float> CreateHannWindow(size_t size) {
        std::vector<float> window(size);
        for (size_t i = 0; i < size; i++) {
            window[i] = 0.5f * (1.0f - std::cos(2.0f * M_PI * i / (size - 1)));
        }
        return window;
    }

    float LinearInterpolate(float a, float b, float t) {
        return a + t * (b - a);
    }

    std::vector<float> SimpleResample(const std::vector<float>& samples, int from_rate, int to_rate) {
        if (from_rate == to_rate) {
            return samples;
        }

        double ratio = static_cast<double>(to_rate) / from_rate;
        size_t new_size = static_cast<size_t>(samples.size() * ratio);
        std::vector<float> output(new_size);

        for (size_t i = 0; i < new_size; i++) {
            double src_index = i / ratio;
            size_t index = static_cast<size_t>(src_index);

            if (index + 1 < samples.size()) {
                float t = src_index - index;
                output[i] = LinearInterpolate(samples[index], samples[index + 1], t);
            } else {
                output[i] = samples[std::min(index, samples.size() - 1)];
            }
        }

        return output;
    }
};

// ==========================================
// Public Interface Implementation
// ==========================================

AudioProcessor::AudioProcessor(int sample_rate)
    : pImpl(std::make_unique<Impl>(sample_rate)) {}

AudioProcessor::~AudioProcessor() = default;
AudioProcessor::AudioProcessor(AudioProcessor&&) noexcept = default;
AudioProcessor& AudioProcessor::operator=(AudioProcessor&&) noexcept = default;

std::vector<float> AudioProcessor::ProcessAudio(
    const std::vector<float>& samples,
    float volume,
    bool normalize) {

    std::vector<float> result = samples;

    // Apply volume
    if (volume != 1.0f) {
        result = ApplyVolume(result, volume);
    }

    // Apply normalization
    if (normalize) {
        result = Normalize(result);
    }

    return result;
}

std::vector<float> AudioProcessor::Normalize(const std::vector<float>& samples) {
    if (samples.empty()) {
        return samples;
    }

    // Find peak amplitude
    float peak = 0.0f;
    for (float sample : samples) {
        peak = std::max(peak, std::abs(sample));
    }

    if (peak == 0.0f) {
        return samples;
    }

    // Normalize to 0.95 to prevent clipping
    float scale = 0.95f / peak;
    std::vector<float> result(samples.size());

    for (size_t i = 0; i < samples.size(); i++) {
        result[i] = samples[i] * scale;
    }

    return result;
}

std::vector<float> AudioProcessor::ApplyVolume(const std::vector<float>& samples, float volume) {
    std::vector<float> result(samples.size());

    for (size_t i = 0; i < samples.size(); i++) {
        result[i] = samples[i] * volume;
    }

    return result;
}

std::vector<float> AudioProcessor::TrimSilence(const std::vector<float>& samples, float threshold) {
    if (samples.empty()) {
        return samples;
    }

    // Find start of audio (first sample above threshold)
    size_t start = 0;
    for (size_t i = 0; i < samples.size(); i++) {
        if (std::abs(samples[i]) > threshold) {
            start = i;
            break;
        }
    }

    // Find end of audio (last sample above threshold)
    size_t end = samples.size() - 1;
    for (size_t i = samples.size() - 1; i > start; i--) {
        if (std::abs(samples[i]) > threshold) {
            end = i;
            break;
        }
    }

    // Return trimmed samples
    if (end > start) {
        return std::vector<float>(samples.begin() + start, samples.begin() + end + 1);
    }

    return samples;
}

std::vector<float> AudioProcessor::ApplyFade(const std::vector<float>& samples, int fade_ms) {
    if (samples.empty() || fade_ms <= 0) {
        return samples;
    }

    std::vector<float> result = samples;
    size_t fade_samples = static_cast<size_t>((fade_ms * pImpl->sample_rate) / 1000);
    fade_samples = std::min(fade_samples, samples.size() / 2);

    // Fade in
    for (size_t i = 0; i < fade_samples && i < result.size(); i++) {
        float factor = static_cast<float>(i) / fade_samples;
        result[i] *= factor;
    }

    // Fade out
    for (size_t i = 0; i < fade_samples && i < result.size(); i++) {
        size_t index = result.size() - 1 - i;
        float factor = static_cast<float>(i) / fade_samples;
        result[index] *= factor;
    }

    return result;
}

std::vector<float> AudioProcessor::Resample(const std::vector<float>& samples,
                                            int from_rate,
                                            int to_rate) {
    return pImpl->SimpleResample(samples, from_rate, to_rate);
}

std::vector<int16_t> AudioProcessor::ToPCM16(const std::vector<float>& samples) {
    std::vector<int16_t> result(samples.size());

    for (size_t i = 0; i < samples.size(); i++) {
        float clamped = std::max(-1.0f, std::min(1.0f, samples[i]));
        result[i] = static_cast<int16_t>(clamped * 32767.0f);
    }

    return result;
}

std::vector<float> AudioProcessor::FromPCM16(const std::vector<int16_t>& pcm) {
    std::vector<float> result(pcm.size());

    for (size_t i = 0; i < pcm.size(); i++) {
        result[i] = static_cast<float>(pcm[i]) / 32767.0f;
    }

    return result;
}

Status AudioProcessor::SaveToFile(const AudioData& audio,
                                  const std::string& filepath,
                                  AudioFormat format) {
    // Convert float samples to PCM16 for WAV writing
    auto pcm_samples = ToPCM16(audio.samples);

    bool success = false;
    if (format == AudioFormat::WAV_PCM16) {
        success = WAVWriter::WritePCM16(filepath, pcm_samples, audio.sample_rate, audio.channels);
    } else {
        success = WAVWriter::WriteFloat(filepath, audio.samples, audio.sample_rate, audio.channels, 16);
    }

    return success ? Status::OK : Status::ERROR_IO;
}

AudioData AudioProcessor::LoadFromFile(const std::string& filepath) {
    AudioData result;

    // Use WAVWriter to read the file
    bool success = WAVWriter::ReadWav(filepath, result.samples, result.sample_rate, result.channels);

    if (!success) {
        result.samples.clear();
        result.sample_rate = 0;
        result.channels = 0;
    }

    return result;
}

std::vector<uint8_t> AudioProcessor::ToWavBytes(const AudioData& audio, AudioFormat format) {
    if (format == AudioFormat::WAV_PCM16) {
        auto pcm_samples = ToPCM16(audio.samples);
        return WAVWriter::CreateWavBytes(pcm_samples, audio.sample_rate, audio.channels);
    } else {
        return WAVWriter::CreateWavBytesFloat(audio.samples, audio.sample_rate, audio.channels, 32);
    }
}

float AudioProcessor::GetRMS(const std::vector<float>& samples) {
    if (samples.empty()) {
        return 0.0f;
    }

    float sum_squares = 0.0f;
    for (float sample : samples) {
        sum_squares += sample * sample;
    }

    return std::sqrt(sum_squares / samples.size());
}

float AudioProcessor::GetPeakLevel(const std::vector<float>& samples) {
    if (samples.empty()) {
        return 0.0f;
    }

    float peak = 0.0f;
    for (float sample : samples) {
        peak = std::max(peak, std::abs(sample));
    }

    return peak;
}

std::vector<float> AudioProcessor::ApplyPitchShift(const std::vector<float>& samples,
                                                   float pitch_factor) {
    // Simple pitch shift using time-domain PSOLA-like approach
    // For a real implementation, this would use more sophisticated algorithms

    if (pitch_factor == 1.0f) {
        return samples;
    }

    // Simple approach: resample and then time-stretch to maintain duration
    int new_rate = static_cast<int>(pImpl->sample_rate / pitch_factor);
    auto resampled = Resample(samples, pImpl->sample_rate, new_rate);

    // Time-stretch back to original length
    return Resample(resampled, new_rate, pImpl->sample_rate);
}

std::vector<float> AudioProcessor::ApplySpeedChange(const std::vector<float>& samples,
                                                    float speed_factor) {
    if (speed_factor == 1.0f) {
        return samples;
    }

    // Speed change by resampling
    int new_rate = static_cast<int>(pImpl->sample_rate * speed_factor);
    return Resample(samples, pImpl->sample_rate, new_rate);
}

} // namespace jp_edge_tts