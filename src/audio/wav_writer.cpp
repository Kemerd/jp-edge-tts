/**
 * @file wav_writer.cpp
 * @brief Implementation of WAV file writer
 * @author D Everett Hinton
 * @date 2025
 *
 * @copyright MIT License
 */

#include "jp_edge_tts/audio/wav_writer.h"
#include <fstream>
#include <algorithm>
#include <iostream>
#include <cstring>

namespace jp_edge_tts {

// ==========================================
// Helper Functions
// ==========================================

namespace {
    template<typename T>
    T Clamp(float value, T min_val, T max_val) {
        if (value < min_val) return min_val;
        if (value > max_val) return max_val;
        return static_cast<T>(value);
    }

    int16_t FloatToPCM16(float value) {
        // Scale float [-1, 1] to int16 range
        float scaled = value * 32767.0f;
        return Clamp<int16_t>(scaled, -32768, 32767);
    }
}

// ==========================================
// Public Interface Implementation
// ==========================================

bool WAVWriter::WritePCM16(const std::string& filepath,
                           const std::vector<int16_t>& samples,
                           int sample_rate,
                           int channels) {
    if (samples.empty()) {
        return false;
    }

    std::ofstream file(filepath, std::ios::binary);
    if (!file) {
        return false;
    }

    // Calculate header values
    WavHeader header;
    header.sample_rate = sample_rate;
    header.num_channels = channels;
    header.bits_per_sample = 16;
    header.data_size = samples.size() * sizeof(int16_t);
    header.Calculate();

    // Write header
    file.write(reinterpret_cast<const char*>(&header), sizeof(WavHeader));

    // Write samples
    file.write(reinterpret_cast<const char*>(samples.data()), samples.size() * sizeof(int16_t));

    return file.good();
}

bool WAVWriter::WriteFloat(const std::string& filepath,
                          const std::vector<float>& samples,
                          int sample_rate,
                          int channels,
                          int bits_per_sample) {
    if (samples.empty()) {
        return false;
    }

    std::ofstream file(filepath, std::ios::binary);
    if (!file) {
        return false;
    }

    // Calculate header values
    WavHeader header;
    header.sample_rate = sample_rate;
    header.num_channels = channels;
    header.bits_per_sample = bits_per_sample;

    if (bits_per_sample == 16) {
        header.audio_format = 1; // PCM
        header.data_size = samples.size() * sizeof(int16_t);
    } else {
        header.audio_format = 3; // IEEE float
        header.data_size = samples.size() * sizeof(float);
    }

    header.Calculate();

    // Write header
    file.write(reinterpret_cast<const char*>(&header), sizeof(WavHeader));

    // Write samples
    if (bits_per_sample == 16) {
        // Convert to PCM16
        for (float sample : samples) {
            int16_t pcm_sample = FloatToPCM16(sample);
            file.write(reinterpret_cast<const char*>(&pcm_sample), sizeof(int16_t));
        }
    } else {
        // Write as float
        file.write(reinterpret_cast<const char*>(samples.data()), samples.size() * sizeof(float));
    }

    return file.good();
}

std::vector<uint8_t> WAVWriter::CreateWavBytes(const std::vector<int16_t>& samples,
                                              int sample_rate,
                                              int channels) {
    if (samples.empty()) {
        return {};
    }

    std::vector<uint8_t> buffer;

    // Calculate header values
    WavHeader header;
    header.sample_rate = sample_rate;
    header.num_channels = channels;
    header.bits_per_sample = 16;
    header.data_size = samples.size() * sizeof(int16_t);
    header.Calculate();

    // Reserve space
    buffer.reserve(sizeof(WavHeader) + header.data_size);

    // Add header
    const uint8_t* header_ptr = reinterpret_cast<const uint8_t*>(&header);
    buffer.insert(buffer.end(), header_ptr, header_ptr + sizeof(WavHeader));

    // Add samples
    const uint8_t* samples_ptr = reinterpret_cast<const uint8_t*>(samples.data());
    buffer.insert(buffer.end(), samples_ptr, samples_ptr + header.data_size);

    return buffer;
}

std::vector<uint8_t> WAVWriter::CreateWavBytesFloat(const std::vector<float>& samples,
                                                   int sample_rate,
                                                   int channels,
                                                   int bits_per_sample) {
    if (samples.empty()) {
        return {};
    }

    std::vector<uint8_t> buffer;

    // Calculate header values
    WavHeader header;
    header.sample_rate = sample_rate;
    header.num_channels = channels;
    header.bits_per_sample = bits_per_sample;

    if (bits_per_sample == 16) {
        header.audio_format = 1; // PCM
        header.data_size = samples.size() * sizeof(int16_t);
    } else {
        header.audio_format = 3; // IEEE float
        header.data_size = samples.size() * sizeof(float);
    }

    header.Calculate();

    // Reserve space
    buffer.reserve(sizeof(WavHeader) + header.data_size);

    // Add header
    const uint8_t* header_ptr = reinterpret_cast<const uint8_t*>(&header);
    buffer.insert(buffer.end(), header_ptr, header_ptr + sizeof(WavHeader));

    // Add samples
    if (bits_per_sample == 16) {
        // Convert to PCM16
        for (float sample : samples) {
            int16_t pcm_sample = FloatToPCM16(sample);
            const uint8_t* sample_ptr = reinterpret_cast<const uint8_t*>(&pcm_sample);
            buffer.insert(buffer.end(), sample_ptr, sample_ptr + sizeof(int16_t));
        }
    } else {
        // Add as float
        const uint8_t* samples_ptr = reinterpret_cast<const uint8_t*>(samples.data());
        buffer.insert(buffer.end(), samples_ptr, samples_ptr + header.data_size);
    }

    return buffer;
}

bool WAVWriter::ReadWav(const std::string& filepath,
                       std::vector<float>& samples,
                       int& sample_rate,
                       int& channels) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        return false;
    }

    // Read header
    WavHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(WavHeader));

    if (!ValidateHeader(header)) {
        return false;
    }

    sample_rate = header.sample_rate;
    channels = header.num_channels;

    // Read samples
    size_t num_samples = header.data_size / (header.bits_per_sample / 8);
    samples.resize(num_samples);

    if (header.bits_per_sample == 16) {
        std::vector<int16_t> pcm_samples(num_samples);
        file.read(reinterpret_cast<char*>(pcm_samples.data()), header.data_size);

        // Convert to float
        for (size_t i = 0; i < num_samples; ++i) {
            samples[i] = static_cast<float>(pcm_samples[i]) / 32767.0f;
        }
    } else if (header.bits_per_sample == 32 && header.audio_format == 3) {
        // Read as float
        file.read(reinterpret_cast<char*>(samples.data()), header.data_size);
    } else {
        return false; // Unsupported format
    }

    return file.good();
}

bool WAVWriter::ValidateHeader(const WavHeader& header) {
    // Check RIFF signature
    if (std::memcmp(header.riff_id, "RIFF", 4) != 0) {
        return false;
    }

    // Check WAVE signature
    if (std::memcmp(header.wave_id, "WAVE", 4) != 0) {
        return false;
    }

    // Check format chunk
    if (std::memcmp(header.fmt_id, "fmt ", 4) != 0) {
        return false;
    }

    // Check data chunk
    if (std::memcmp(header.data_id, "data", 4) != 0) {
        return false;
    }

    // Check audio format (PCM or IEEE float)
    if (header.audio_format != 1 && header.audio_format != 3) {
        return false;
    }

    return true;
}

bool WAVWriter::GetWavInfo(const std::string& filepath,
                          int& sample_rate,
                          int& channels,
                          int& duration_ms) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        return false;
    }

    // Read header
    WavHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(WavHeader));

    if (!ValidateHeader(header)) {
        return false;
    }

    sample_rate = header.sample_rate;
    channels = header.num_channels;

    // Calculate duration
    size_t num_samples = header.data_size / (header.bits_per_sample / 8) / channels;
    duration_ms = static_cast<int>((num_samples * 1000) / sample_rate);

    return true;
}

} // namespace jp_edge_tts