/**
 * @file wav_writer.h
 * @brief WAV file writing utilities
 * D Everett Hinton
 * @date 2025
 *
 * @copyright MIT License
 */

#ifndef JP_EDGE_TTS_WAV_WRITER_H
#define JP_EDGE_TTS_WAV_WRITER_H

#include <vector>
#include <string>
#include <cstdint>

namespace jp_edge_tts {

/**
 * @brief WAV file format enumeration
 */
enum class WAVFormat {
    PCM_16,      // 16-bit PCM
    PCM_24,      // 24-bit PCM
    PCM_32,      // 32-bit PCM
    FLOAT_32     // 32-bit float
};

/**
 * @struct WavHeader
 * @brief WAV file header structure
 */
struct WavHeader {
    // RIFF chunk
    char riff_id[4] = {'R', 'I', 'F', 'F'};
    uint32_t riff_size;
    char wave_id[4] = {'W', 'A', 'V', 'E'};

    // Format chunk
    char fmt_id[4] = {'f', 'm', 't', ' '};
    uint32_t fmt_size = 16;
    uint16_t audio_format = 1;  // PCM
    uint16_t num_channels = 1;
    uint32_t sample_rate = 24000;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample = 16;

    // Data chunk
    char data_id[4] = {'d', 'a', 't', 'a'};
    uint32_t data_size;

    /**
     * @brief Calculate derived fields
     */
    void Calculate() {
        block_align = num_channels * bits_per_sample / 8;
        byte_rate = sample_rate * block_align;
        riff_size = 36 + data_size;
    }
};

/**
 * @class WAVWriter
 * @brief Utility class for writing WAV files
 */
class WAVWriter {
private:
    class Impl;
public:
    /**
     * @brief Write PCM16 samples to WAV file
     *
     * @param filepath Output file path
     * @param samples PCM16 samples
     * @param sample_rate Sample rate in Hz
     * @param channels Number of channels
     * @return true if successful
     */
    static bool WritePCM16(const std::string& filepath,
                          const std::vector<int16_t>& samples,
                          int sample_rate = 24000,
                          int channels = 1);

    /**
     * @brief Write float samples to WAV file
     *
     * @param filepath Output file path
     * @param samples Float samples (-1.0 to 1.0)
     * @param sample_rate Sample rate in Hz
     * @param channels Number of channels
     * @param bits_per_sample Bits per sample (16 or 32)
     * @return true if successful
     */
    static bool WriteFloat(const std::string& filepath,
                          const std::vector<float>& samples,
                          int sample_rate = 24000,
                          int channels = 1,
                          int bits_per_sample = 16);

    /**
     * @brief Create WAV byte array from PCM16 samples
     *
     * @param samples PCM16 samples
     * @param sample_rate Sample rate in Hz
     * @param channels Number of channels
     * @return WAV file as byte array
     */
    static std::vector<uint8_t> CreateWavBytes(
        const std::vector<int16_t>& samples,
        int sample_rate = 24000,
        int channels = 1);

    /**
     * @brief Create WAV byte array from float samples
     *
     * @param samples Float samples
     * @param sample_rate Sample rate in Hz
     * @param channels Number of channels
     * @param bits_per_sample Bits per sample
     * @return WAV file as byte array
     */
    static std::vector<uint8_t> CreateWavBytesFloat(
        const std::vector<float>& samples,
        int sample_rate = 24000,
        int channels = 1,
        int bits_per_sample = 16);

    /**
     * @brief Read WAV file
     *
     * @param filepath Input file path
     * @param samples Output samples
     * @param sample_rate Output sample rate
     * @param channels Output channels
     * @return true if successful
     */
    static bool ReadWav(const std::string& filepath,
                       std::vector<float>& samples,
                       int& sample_rate,
                       int& channels);

    /**
     * @brief Validate WAV header
     *
     * @param header WAV header to validate
     * @return true if valid
     */
    static bool ValidateHeader(const WavHeader& header);

    /**
     * @brief Get WAV file info without loading data
     *
     * @param filepath WAV file path
     * @param sample_rate Output sample rate
     * @param channels Output channels
     * @param duration_ms Output duration in milliseconds
     * @return true if successful
     */
    static bool GetWavInfo(const std::string& filepath,
                          int& sample_rate,
                          int& channels,
                          int& duration_ms);
};

} // namespace jp_edge_tts

#endif // JP_EDGE_TTS_WAV_WRITER_H