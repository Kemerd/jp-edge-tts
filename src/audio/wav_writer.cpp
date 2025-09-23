/**
 * @file wav_writer.cpp
 * @brief Implementation of WAV file writer
 * @author D Everett Hinton
 * @date 2025
 *
 * @copyright MIT License
 */

#include "jp_edge_tts/audio/wav_writer.h"
#include "jp_edge_tts/utils/file_utils.h"
#include <fstream>
#include <cstring>
#include <algorithm>
#include <iostream>

namespace jp_edge_tts {

// ==========================================
// WAV File Format Structures
// ==========================================

#pragma pack(push, 1)

struct WAVHeader {
    // RIFF chunk
    char riff_id[4];        // "RIFF"
    uint32_t riff_size;     // File size - 8
    char wave_id[4];        // "WAVE"
    
    // Format chunk
    char fmt_id[4];         // "fmt "
    uint32_t fmt_size;      // Format chunk size (16 for PCM)
    uint16_t format_tag;    // 1 = PCM, 3 = IEEE float
    uint16_t channels;      // Number of channels
    uint32_t sample_rate;   // Sample rate
    uint32_t byte_rate;     // Sample rate * channels * bits per sample / 8
    uint16_t block_align;   // Channels * bits per sample / 8
    uint16_t bits_per_sample; // Bits per sample
    
    // Data chunk
    char data_id[4];        // "data"
    uint32_t data_size;     // Size of audio data
};

#pragma pack(pop)

// ==========================================
// Private Implementation
// ==========================================

class WAVWriter::Impl {
public:
    WAVFormat default_format = WAVFormat::PCM_16;
    int default_sample_rate = 22050;
    int default_channels = 1;

    bool WriteWAVHeader(std::ofstream& file,
                       uint32_t data_size,
                       int sample_rate,
                       int channels,
                       int bits_per_sample,
                       bool is_float = false) {
        WAVHeader header;
        
        // RIFF chunk
        std::memcpy(header.riff_id, "RIFF", 4);
        header.riff_size = data_size + sizeof(WAVHeader) - 8;
        std::memcpy(header.wave_id, "WAVE", 4);
        
        // Format chunk
        std::memcpy(header.fmt_id, "fmt ", 4);
        header.fmt_size = 16;  // PCM format size
        header.format_tag = is_float ? 3 : 1;  // 1 = PCM, 3 = IEEE float
        header.channels = channels;
        header.sample_rate = sample_rate;
        header.byte_rate = sample_rate * channels * bits_per_sample / 8;
        header.block_align = channels * bits_per_sample / 8;
        header.bits_per_sample = bits_per_sample;
        
        // Data chunk
        std::memcpy(header.data_id, "data", 4);
        header.data_size = data_size;
        
        // Write header
        file.write(reinterpret_cast<const char*>(&header), sizeof(WAVHeader));
        
        return file.good();
    }

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

    int32_t FloatToPCM24(float value) {
        // Scale float [-1, 1] to 24-bit range
        float scaled = value * 8388607.0f;
        return Clamp<int32_t>(scaled, -8388608, 8388607);
    }

    int32_t FloatToPCM32(float value) {
        // Scale float [-1, 1] to int32 range
        float scaled = value * 2147483647.0f;
        return Clamp<int32_t>(scaled, -2147483648LL, 2147483647);
    }
};

// ==========================================
// Public Interface Implementation
// ==========================================

WAVWriter::WAVWriter() : pImpl(std::make_unique<Impl>()) {}
WAVWriter::~WAVWriter() = default;
WAVWriter::WAVWriter(WAVWriter&&) noexcept = default;
WAVWriter& WAVWriter::operator=(WAVWriter&&) noexcept = default;

bool WAVWriter::WriteWAV(
    const std::string& filename,
    const std::vector<float>& audio_data,
    int sample_rate,
    int channels,
    WAVFormat format) {
    
    if (audio_data.empty()) {
        std::cerr << "Error: Empty audio data" << std::endl;
        return false;
    }
    
    // Ensure directory exists
    std::string directory = FileUtils::GetDirectory(filename);
    if (!directory.empty() && !FileUtils::Exists(directory)) {
        FileUtils::CreateDirectories(directory);
    }
    
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Cannot open file for writing: " << filename << std::endl;
        return false;
    }
    
    // Calculate data size based on format
    uint32_t data_size = 0;
    int bits_per_sample = 0;
    
    switch (format) {
        case WAVFormat::PCM_16:
            bits_per_sample = 16;
            data_size = audio_data.size() * sizeof(int16_t);
            break;
        case WAVFormat::PCM_24:
            bits_per_sample = 24;
            data_size = audio_data.size() * 3;  // 24 bits = 3 bytes
            break;
        case WAVFormat::PCM_32:
            bits_per_sample = 32;
            data_size = audio_data.size() * sizeof(int32_t);
            break;
        case WAVFormat::FLOAT_32:
            bits_per_sample = 32;
            data_size = audio_data.size() * sizeof(float);
            break;
        default:
            std::cerr << "Error: Unsupported WAV format" << std::endl;
            return false;
    }
    
    // Write header
    bool is_float = (format == WAVFormat::FLOAT_32);
    if (!pImpl->WriteWAVHeader(file, data_size, sample_rate, channels, bits_per_sample, is_float)) {
        std::cerr << "Error: Failed to write WAV header" << std::endl;
        return false;
    }
    
    // Write audio data based on format
    switch (format) {
        case WAVFormat::PCM_16: {
            for (float sample : audio_data) {
                int16_t pcm_sample = pImpl->FloatToPCM16(sample);
                file.write(reinterpret_cast<const char*>(&pcm_sample), sizeof(int16_t));
            }
            break;
        }
        
        case WAVFormat::PCM_24: {
            for (float sample : audio_data) {
                int32_t pcm_sample = pImpl->FloatToPCM24(sample);
                // Write only the lower 3 bytes (24 bits)
                uint8_t bytes[3];
                bytes[0] = (pcm_sample >> 0) & 0xFF;
                bytes[1] = (pcm_sample >> 8) & 0xFF;
                bytes[2] = (pcm_sample >> 16) & 0xFF;
                file.write(reinterpret_cast<const char*>(bytes), 3);
            }
            break;
        }
        
        case WAVFormat::PCM_32: {
            for (float sample : audio_data) {
                int32_t pcm_sample = pImpl->FloatToPCM32(sample);
                file.write(reinterpret_cast<const char*>(&pcm_sample), sizeof(int32_t));
            }
            break;
        }
        
        case WAVFormat::FLOAT_32: {
            // Write float samples directly
            file.write(reinterpret_cast<const char*>(audio_data.data()),
                      audio_data.size() * sizeof(float));
            break;
        }
    }
    
    file.close();
    
    if (!file.good()) {
        std::cerr << "Error: Failed to write audio data" << std::endl;
        return false;
    }
    
    return true;
}

std::vector<uint8_t> WAVWriter::CreateWAVBuffer(
    const std::vector<float>& audio_data,
    int sample_rate,
    int channels,
    WAVFormat format) {
    
    if (audio_data.empty()) {
        return {};
    }
    
    std::vector<uint8_t> buffer;
    
    // Calculate sizes
    uint32_t data_size = 0;
    int bits_per_sample = 0;
    
    switch (format) {
        case WAVFormat::PCM_16:
            bits_per_sample = 16;
            data_size = audio_data.size() * sizeof(int16_t);
            break;
        case WAVFormat::PCM_24:
            bits_per_sample = 24;
            data_size = audio_data.size() * 3;
            break;
        case WAVFormat::PCM_32:
            bits_per_sample = 32;
            data_size = audio_data.size() * sizeof(int32_t);
            break;
        case WAVFormat::FLOAT_32:
            bits_per_sample = 32;
            data_size = audio_data.size() * sizeof(float);
            break;
    }
    
    // Reserve space for header and data
    buffer.reserve(sizeof(WAVHeader) + data_size);
    
    // Create header
    WAVHeader header;
    
    // RIFF chunk
    std::memcpy(header.riff_id, "RIFF", 4);
    header.riff_size = data_size + sizeof(WAVHeader) - 8;
    std::memcpy(header.wave_id, "WAVE", 4);
    
    // Format chunk
    std::memcpy(header.fmt_id, "fmt ", 4);
    header.fmt_size = 16;
    header.format_tag = (format == WAVFormat::FLOAT_32) ? 3 : 1;
    header.channels = channels;
    header.sample_rate = sample_rate;
    header.byte_rate = sample_rate * channels * bits_per_sample / 8;
    header.block_align = channels * bits_per_sample / 8;
    header.bits_per_sample = bits_per_sample;
    
    // Data chunk
    std::memcpy(header.data_id, "data", 4);
    header.data_size = data_size;
    
    // Add header to buffer
    const uint8_t* header_ptr = reinterpret_cast<const uint8_t*>(&header);
    buffer.insert(buffer.end(), header_ptr, header_ptr + sizeof(WAVHeader));
    
    // Add audio data based on format
    switch (format) {
        case WAVFormat::PCM_16: {
            for (float sample : audio_data) {
                int16_t pcm_sample = pImpl->FloatToPCM16(sample);
                const uint8_t* sample_ptr = reinterpret_cast<const uint8_t*>(&pcm_sample);
                buffer.insert(buffer.end(), sample_ptr, sample_ptr + sizeof(int16_t));
            }
            break;
        }
        
        case WAVFormat::PCM_24: {
            for (float sample : audio_data) {
                int32_t pcm_sample = pImpl->FloatToPCM24(sample);
                buffer.push_back((pcm_sample >> 0) & 0xFF);
                buffer.push_back((pcm_sample >> 8) & 0xFF);
                buffer.push_back((pcm_sample >> 16) & 0xFF);
            }
            break;
        }
        
        case WAVFormat::PCM_32: {
            for (float sample : audio_data) {
                int32_t pcm_sample = pImpl->FloatToPCM32(sample);
                const uint8_t* sample_ptr = reinterpret_cast<const uint8_t*>(&pcm_sample);
                buffer.insert(buffer.end(), sample_ptr, sample_ptr + sizeof(int32_t));
            }
            break;
        }
        
        case WAVFormat::FLOAT_32: {
            const uint8_t* data_ptr = reinterpret_cast<const uint8_t*>(audio_data.data());
            buffer.insert(buffer.end(), data_ptr, data_ptr + audio_data.size() * sizeof(float));
            break;
        }
    }
    
    return buffer;
}

bool WAVWriter::WriteRawPCM(
    const std::string& filename,
    const std::vector<int16_t>& pcm_data,
    int sample_rate,
    int channels) {
    
    if (pcm_data.empty()) {
        return false;
    }
    
    // Ensure directory exists
    std::string directory = FileUtils::GetDirectory(filename);
    if (!directory.empty() && !FileUtils::Exists(directory)) {
        FileUtils::CreateDirectories(directory);
    }
    
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        return false;
    }
    
    // Write WAV header
    uint32_t data_size = pcm_data.size() * sizeof(int16_t);
    if (!pImpl->WriteWAVHeader(file, data_size, sample_rate, channels, 16, false)) {
        return false;
    }
    
    // Write PCM data
    file.write(reinterpret_cast<const char*>(pcm_data.data()),
              pcm_data.size() * sizeof(int16_t));
    
    return file.good();
}

void WAVWriter::SetDefaultFormat(WAVFormat format) {
    pImpl->default_format = format;
}

void WAVWriter::SetDefaultSampleRate(int sample_rate) {
    pImpl->default_sample_rate = sample_rate;
}

void WAVWriter::SetDefaultChannels(int channels) {
    pImpl->default_channels = channels;
}

bool WAVWriter::ConvertFormat(
    const std::string& input_file,
    const std::string& output_file,
    WAVFormat target_format) {
    
    // This is a placeholder for format conversion
    // In a real implementation, we would:
    // 1. Read the input WAV file
    // 2. Convert the audio data to the target format
    // 3. Write the output WAV file
    
    // For now, just copy the file
    return FileUtils::CopyFile(input_file, output_file);
}

int64_t WAVWriter::EstimateFileSize(
    size_t sample_count,
    int channels,
    WAVFormat format) {
    
    int64_t data_size = 0;
    
    switch (format) {
        case WAVFormat::PCM_16:
            data_size = sample_count * channels * 2;  // 2 bytes per sample
            break;
        case WAVFormat::PCM_24:
            data_size = sample_count * channels * 3;  // 3 bytes per sample
            break;
        case WAVFormat::PCM_32:
        case WAVFormat::FLOAT_32:
            data_size = sample_count * channels * 4;  // 4 bytes per sample
            break;
    }
    
    // Add header size
    return data_size + sizeof(WAVHeader);
}

} // namespace jp_edge_tts