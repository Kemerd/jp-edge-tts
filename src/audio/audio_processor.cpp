/**
 * @file audio_processor.cpp
 * @brief Implementation of audio processing and manipulation
 * @author D Everett Hinton
 * @date 2025
 *
 * @copyright MIT License
 */

#include "jp_edge_tts/audio/audio_processor.h"
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
    // Default parameters
    int default_sample_rate = 22050;
    float default_volume = 1.0f;
    
    // Window functions for spectral processing
    std::vector<float> CreateHannWindow(size_t size) {
        std::vector<float> window(size);
        for (size_t i = 0; i < size; i++) {
            window[i] = 0.5f * (1.0f - std::cos(2.0f * M_PI * i / (size - 1)));
        }
        return window;
    }
    
    std::vector<float> CreateHammingWindow(size_t size) {
        std::vector<float> window(size);
        for (size_t i = 0; i < size; i++) {
            window[i] = 0.54f - 0.46f * std::cos(2.0f * M_PI * i / (size - 1));
        }
        return window;
    }
    
    // Linear interpolation for resampling
    float LinearInterpolate(float y1, float y2, float mu) {
        return y1 * (1.0f - mu) + y2 * mu;
    }
    
    // Cubic interpolation for higher quality resampling
    float CubicInterpolate(float y0, float y1, float y2, float y3, float mu) {
        float mu2 = mu * mu;
        float a0 = y3 - y2 - y0 + y1;
        float a1 = y0 - y1 - a0;
        float a2 = y2 - y0;
        float a3 = y1;
        
        return a0 * mu * mu2 + a1 * mu2 + a2 * mu + a3;
    }
    
    // Apply pre-emphasis filter
    std::vector<float> ApplyPreEmphasis(const std::vector<float>& audio, float coefficient) {
        if (audio.empty()) return audio;
        
        std::vector<float> result(audio.size());
        result[0] = audio[0];
        
        for (size_t i = 1; i < audio.size(); i++) {
            result[i] = audio[i] - coefficient * audio[i - 1];
        }
        
        return result;
    }
    
    // Remove pre-emphasis (de-emphasis)
    std::vector<float> RemovePreEmphasis(const std::vector<float>& audio, float coefficient) {
        if (audio.empty()) return audio;
        
        std::vector<float> result(audio.size());
        result[0] = audio[0];
        
        for (size_t i = 1; i < audio.size(); i++) {
            result[i] = audio[i] + coefficient * result[i - 1];
        }
        
        return result;
    }
};

// ==========================================
// Public Interface Implementation
// ==========================================

AudioProcessor::AudioProcessor() : pImpl(std::make_unique<Impl>()) {}
AudioProcessor::~AudioProcessor() = default;
AudioProcessor::AudioProcessor(AudioProcessor&&) noexcept = default;
AudioProcessor& AudioProcessor::operator=(AudioProcessor&&) noexcept = default;

std::vector<float> AudioProcessor::Normalize(
    const std::vector<float>& audio,
    float target_level,
    bool prevent_clipping) {
    
    if (audio.empty()) {
        return audio;
    }
    
    // Find maximum absolute value
    float max_abs = 0.0f;
    for (float sample : audio) {
        max_abs = std::max(max_abs, std::abs(sample));
    }
    
    // Avoid division by zero
    if (max_abs == 0.0f) {
        return audio;
    }
    
    // Calculate scaling factor
    float scale = target_level / max_abs;
    
    // Apply scaling
    std::vector<float> result(audio.size());
    for (size_t i = 0; i < audio.size(); i++) {
        result[i] = audio[i] * scale;
        
        // Prevent clipping if requested
        if (prevent_clipping) {
            result[i] = std::max(-1.0f, std::min(1.0f, result[i]));
        }
    }
    
    return result;
}

std::vector<float> AudioProcessor::ApplyVolume(
    const std::vector<float>& audio,
    float volume) {
    
    if (audio.empty() || volume == 1.0f) {
        return audio;
    }
    
    std::vector<float> result(audio.size());
    
    for (size_t i = 0; i < audio.size(); i++) {
        result[i] = audio[i] * volume;
        // Soft clipping to prevent harsh distortion
        if (result[i] > 1.0f) {
            result[i] = std::tanh(result[i]);
        } else if (result[i] < -1.0f) {
            result[i] = std::tanh(result[i]);
        }
    }
    
    return result;
}

std::vector<float> AudioProcessor::Resample(
    const std::vector<float>& audio,
    int original_rate,
    int target_rate,
    ResampleQuality quality) {
    
    if (audio.empty() || original_rate == target_rate) {
        return audio;
    }
    
    // Calculate resampling ratio
    double ratio = static_cast<double>(original_rate) / target_rate;
    size_t new_length = static_cast<size_t>(audio.size() / ratio);
    
    std::vector<float> result(new_length);
    
    if (quality == ResampleQuality::LOW) {
        // Nearest neighbor (fastest, lowest quality)
        for (size_t i = 0; i < new_length; i++) {
            size_t src_idx = static_cast<size_t>(i * ratio);
            src_idx = std::min(src_idx, audio.size() - 1);
            result[i] = audio[src_idx];
        }
    } else if (quality == ResampleQuality::MEDIUM) {
        // Linear interpolation
        for (size_t i = 0; i < new_length; i++) {
            double src_pos = i * ratio;
            size_t src_idx = static_cast<size_t>(src_pos);
            double mu = src_pos - src_idx;
            
            if (src_idx + 1 < audio.size()) {
                result[i] = pImpl->LinearInterpolate(audio[src_idx], audio[src_idx + 1], mu);
            } else {
                result[i] = audio[src_idx];
            }
        }
    } else {
        // Cubic interpolation (best quality)
        for (size_t i = 0; i < new_length; i++) {
            double src_pos = i * ratio;
            size_t src_idx = static_cast<size_t>(src_pos);
            double mu = src_pos - src_idx;
            
            // Get four points for cubic interpolation
            float y0 = (src_idx > 0) ? audio[src_idx - 1] : audio[src_idx];
            float y1 = audio[src_idx];
            float y2 = (src_idx + 1 < audio.size()) ? audio[src_idx + 1] : audio[src_idx];
            float y3 = (src_idx + 2 < audio.size()) ? audio[src_idx + 2] : y2;
            
            result[i] = pImpl->CubicInterpolate(y0, y1, y2, y3, mu);
        }
    }
    
    return result;
}

std::vector<float> AudioProcessor::ChangePitch(
    const std::vector<float>& audio,
    float pitch_factor,
    int sample_rate) {
    
    if (audio.empty() || pitch_factor == 1.0f) {
        return audio;
    }
    
    // Simple pitch shifting using resampling
    // Note: This changes duration. For pitch shift without duration change,
    // we would need PSOLA or phase vocoder
    
    int new_rate = static_cast<int>(sample_rate * pitch_factor);
    return Resample(audio, new_rate, sample_rate, ResampleQuality::HIGH);
}

std::vector<float> AudioProcessor::ChangeSpeed(
    const std::vector<float>& audio,
    float speed_factor) {
    
    if (audio.empty() || speed_factor == 1.0f) {
        return audio;
    }
    
    // Speed change by resampling
    size_t new_length = static_cast<size_t>(audio.size() / speed_factor);
    std::vector<float> result(new_length);
    
    for (size_t i = 0; i < new_length; i++) {
        float src_pos = i * speed_factor;
        size_t src_idx = static_cast<size_t>(src_pos);
        float mu = src_pos - src_idx;
        
        if (src_idx + 1 < audio.size()) {
            result[i] = pImpl->LinearInterpolate(audio[src_idx], audio[src_idx + 1], mu);
        } else if (src_idx < audio.size()) {
            result[i] = audio[src_idx];
        } else {
            result[i] = 0.0f;
        }
    }
    
    return result;
}

std::vector<float> AudioProcessor::ApplyFadeIn(
    const std::vector<float>& audio,
    float duration_seconds,
    int sample_rate) {
    
    if (audio.empty() || duration_seconds <= 0) {
        return audio;
    }
    
    size_t fade_samples = static_cast<size_t>(duration_seconds * sample_rate);
    fade_samples = std::min(fade_samples, audio.size());
    
    std::vector<float> result = audio;
    
    for (size_t i = 0; i < fade_samples; i++) {
        float fade_factor = static_cast<float>(i) / fade_samples;
        result[i] *= fade_factor;
    }
    
    return result;
}

std::vector<float> AudioProcessor::ApplyFadeOut(
    const std::vector<float>& audio,
    float duration_seconds,
    int sample_rate) {
    
    if (audio.empty() || duration_seconds <= 0) {
        return audio;
    }
    
    size_t fade_samples = static_cast<size_t>(duration_seconds * sample_rate);
    fade_samples = std::min(fade_samples, audio.size());
    
    std::vector<float> result = audio;
    size_t start_idx = audio.size() - fade_samples;
    
    for (size_t i = 0; i < fade_samples; i++) {
        float fade_factor = 1.0f - (static_cast<float>(i) / fade_samples);
        result[start_idx + i] *= fade_factor;
    }
    
    return result;
}

std::vector<float> AudioProcessor::Concatenate(
    const std::vector<std::vector<float>>& audio_segments,
    float gap_seconds,
    int sample_rate) {
    
    if (audio_segments.empty()) {
        return {};
    }
    
    // Calculate total size
    size_t gap_samples = static_cast<size_t>(gap_seconds * sample_rate);
    size_t total_size = 0;
    
    for (const auto& segment : audio_segments) {
        total_size += segment.size();
    }
    total_size += gap_samples * (audio_segments.size() - 1);
    
    // Concatenate with gaps
    std::vector<float> result;
    result.reserve(total_size);
    
    for (size_t i = 0; i < audio_segments.size(); i++) {
        // Add segment
        result.insert(result.end(), audio_segments[i].begin(), audio_segments[i].end());
        
        // Add gap (silence) between segments
        if (i < audio_segments.size() - 1) {
            result.insert(result.end(), gap_samples, 0.0f);
        }
    }
    
    return result;
}

std::vector<float> AudioProcessor::Mix(
    const std::vector<float>& audio1,
    const std::vector<float>& audio2,
    float mix_ratio) {
    
    if (audio1.empty()) return audio2;
    if (audio2.empty()) return audio1;
    
    // Ensure mix ratio is in valid range
    mix_ratio = std::max(0.0f, std::min(1.0f, mix_ratio));
    
    size_t max_length = std::max(audio1.size(), audio2.size());
    std::vector<float> result(max_length);
    
    for (size_t i = 0; i < max_length; i++) {
        float sample1 = (i < audio1.size()) ? audio1[i] : 0.0f;
        float sample2 = (i < audio2.size()) ? audio2[i] : 0.0f;
        
        result[i] = sample1 * (1.0f - mix_ratio) + sample2 * mix_ratio;
    }
    
    return result;
}

std::vector<float> AudioProcessor::RemoveSilence(
    const std::vector<float>& audio,
    float threshold,
    int sample_rate) {
    
    if (audio.empty()) {
        return audio;
    }
    
    // Find first non-silent sample
    size_t start_idx = 0;
    for (size_t i = 0; i < audio.size(); i++) {
        if (std::abs(audio[i]) > threshold) {
            start_idx = i;
            break;
        }
    }
    
    // Find last non-silent sample
    size_t end_idx = audio.size() - 1;
    for (size_t i = audio.size() - 1; i > start_idx; i--) {
        if (std::abs(audio[i]) > threshold) {
            end_idx = i;
            break;
        }
    }
    
    // Extract non-silent portion
    if (start_idx < end_idx) {
        return std::vector<float>(audio.begin() + start_idx, audio.begin() + end_idx + 1);
    }
    
    return audio;
}

std::vector<float> AudioProcessor::AddNoise(
    const std::vector<float>& audio,
    float noise_level,
    NoiseType type) {
    
    if (audio.empty() || noise_level <= 0) {
        return audio;
    }
    
    std::vector<float> result = audio;
    
    // Simple random number generator for noise
    unsigned int seed = 42;
    auto rand_float = [&seed]() -> float {
        seed = (seed * 1103515245 + 12345) & 0x7fffffff;
        return (seed / (float)0x7fffffff) * 2.0f - 1.0f;
    };
    
    if (type == NoiseType::WHITE) {
        // White noise: random values
        for (size_t i = 0; i < audio.size(); i++) {
            result[i] += rand_float() * noise_level;
        }
    } else if (type == NoiseType::PINK) {
        // Simplified pink noise using running average
        float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f;
        
        for (size_t i = 0; i < audio.size(); i++) {
            float white = rand_float();
            b0 = 0.99886f * b0 + white * 0.0555179f;
            b1 = 0.99332f * b1 + white * 0.0750759f;
            b2 = 0.96900f * b2 + white * 0.1538520f;
            float pink = (b0 + b1 + b2 + white * 0.5362f) * 0.2f;
            result[i] += pink * noise_level;
        }
    }
    
    return result;
}

std::vector<float> AudioProcessor::ConvertToMono(
    const std::vector<float>& stereo_audio) {
    
    if (stereo_audio.size() < 2) {
        return stereo_audio;
    }
    
    // Assume interleaved stereo: L, R, L, R, ...
    size_t mono_length = stereo_audio.size() / 2;
    std::vector<float> mono(mono_length);
    
    for (size_t i = 0; i < mono_length; i++) {
        float left = stereo_audio[i * 2];
        float right = stereo_audio[i * 2 + 1];
        mono[i] = (left + right) * 0.5f;
    }
    
    return mono;
}

std::vector<float> AudioProcessor::ConvertToStereo(
    const std::vector<float>& mono_audio,
    float pan) {
    
    if (mono_audio.empty()) {
        return mono_audio;
    }
    
    // Ensure pan is in valid range [-1, 1]
    pan = std::max(-1.0f, std::min(1.0f, pan));
    
    // Calculate left and right gains
    float left_gain = (pan <= 0) ? 1.0f : 1.0f - pan;
    float right_gain = (pan >= 0) ? 1.0f : 1.0f + pan;
    
    // Create interleaved stereo
    std::vector<float> stereo(mono_audio.size() * 2);
    
    for (size_t i = 0; i < mono_audio.size(); i++) {
        stereo[i * 2] = mono_audio[i] * left_gain;      // Left channel
        stereo[i * 2 + 1] = mono_audio[i] * right_gain;  // Right channel
    }
    
    return stereo;
}

AudioInfo AudioProcessor::AnalyzeAudio(const std::vector<float>& audio, int sample_rate) {
    AudioInfo info;
    info.sample_count = audio.size();
    info.duration_seconds = static_cast<float>(audio.size()) / sample_rate;
    info.sample_rate = sample_rate;
    
    if (audio.empty()) {
        return info;
    }
    
    // Calculate statistics
    float sum = 0.0f;
    info.peak_amplitude = 0.0f;
    info.min_value = audio[0];
    info.max_value = audio[0];
    
    for (float sample : audio) {
        sum += std::abs(sample);
        info.peak_amplitude = std::max(info.peak_amplitude, std::abs(sample));
        info.min_value = std::min(info.min_value, sample);
        info.max_value = std::max(info.max_value, sample);
    }
    
    info.average_amplitude = sum / audio.size();
    
    // Calculate RMS (Root Mean Square)
    float sum_squares = 0.0f;
    for (float sample : audio) {
        sum_squares += sample * sample;
    }
    info.rms = std::sqrt(sum_squares / audio.size());
    
    // Estimate if clipping occurred
    info.is_clipped = (info.peak_amplitude >= 0.99f);
    
    return info;
}

} // namespace jp_edge_tts