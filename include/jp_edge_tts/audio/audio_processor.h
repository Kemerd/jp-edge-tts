/**
 * @file audio_processor.h
 * @brief Audio processing and normalization utilities
 * D Everett Hinton
 * @date 2025
 *
 * @copyright MIT License
 */

#ifndef JP_EDGE_TTS_AUDIO_PROCESSOR_H
#define JP_EDGE_TTS_AUDIO_PROCESSOR_H

#include "jp_edge_tts/types.h"
#include <vector>
#include <string>
#include <memory>

namespace jp_edge_tts {

/**
 * @class AudioProcessor
 * @brief Processes and normalizes audio data from TTS models
 *
 * @details Handles audio post-processing including normalization,
 * resampling, and format conversion.
 */
class AudioProcessor {
public:
    /**
     * @brief Constructor
     * @param sample_rate Target sample rate in Hz
     */
    explicit AudioProcessor(int sample_rate = 24000);

    /**
     * @brief Destructor
     */
    ~AudioProcessor();

    // Disable copy, enable move
    AudioProcessor(const AudioProcessor&) = delete;
    AudioProcessor& operator=(const AudioProcessor&) = delete;
    AudioProcessor(AudioProcessor&&) noexcept;
    AudioProcessor& operator=(AudioProcessor&&) noexcept;

    /**
     * @brief Process raw audio samples
     *
     * @param samples Input audio samples
     * @param volume Volume adjustment (0.0-1.0)
     * @param normalize Apply normalization
     * @return Processed audio samples
     */
    std::vector<float> ProcessAudio(
        const std::vector<float>& samples,
        float volume = 1.0f,
        bool normalize = true
    );

    /**
     * @brief Normalize audio to prevent clipping
     *
     * @param samples Audio samples to normalize
     * @return Normalized samples
     */
    std::vector<float> Normalize(const std::vector<float>& samples);

    /**
     * @brief Apply volume adjustment
     *
     * @param samples Input samples
     * @param volume Volume factor (0.0-1.0)
     * @return Volume-adjusted samples
     */
    std::vector<float> ApplyVolume(const std::vector<float>& samples, float volume);

    /**
     * @brief Remove silence from beginning and end
     *
     * @param samples Input samples
     * @param threshold Silence threshold
     * @return Trimmed samples
     */
    std::vector<float> TrimSilence(const std::vector<float>& samples,
                                   float threshold = 0.01f);

    /**
     * @brief Apply fade in/out
     *
     * @param samples Input samples
     * @param fade_ms Fade duration in milliseconds
     * @return Samples with fade applied
     */
    std::vector<float> ApplyFade(const std::vector<float>& samples,
                                 int fade_ms = 50);

    /**
     * @brief Resample audio to different sample rate
     *
     * @param samples Input samples
     * @param from_rate Original sample rate
     * @param to_rate Target sample rate
     * @return Resampled audio
     */
    std::vector<float> Resample(const std::vector<float>& samples,
                                int from_rate,
                                int to_rate);

    /**
     * @brief Convert float samples to PCM16
     *
     * @param samples Float samples (-1.0 to 1.0)
     * @return PCM16 samples
     */
    std::vector<int16_t> ToPCM16(const std::vector<float>& samples);

    /**
     * @brief Convert PCM16 to float samples
     *
     * @param pcm PCM16 samples
     * @return Float samples (-1.0 to 1.0)
     */
    std::vector<float> FromPCM16(const std::vector<int16_t>& pcm);

    /**
     * @brief Save audio to WAV file
     *
     * @param audio Audio data
     * @param filepath Output file path
     * @param format Audio format
     * @return Status code
     */
    Status SaveToFile(const AudioData& audio,
                     const std::string& filepath,
                     AudioFormat format = AudioFormat::WAV_PCM16);

    /**
     * @brief Load audio from WAV file
     *
     * @param filepath Input file path
     * @return Audio data
     */
    AudioData LoadFromFile(const std::string& filepath);

    /**
     * @brief Convert audio to WAV byte array
     *
     * @param audio Audio data
     * @param format Output format
     * @return WAV file as byte array
     */
    std::vector<uint8_t> ToWavBytes(const AudioData& audio,
                                    AudioFormat format = AudioFormat::WAV_PCM16);

    /**
     * @brief Get RMS (Root Mean Square) level
     *
     * @param samples Audio samples
     * @return RMS level
     */
    float GetRMS(const std::vector<float>& samples);

    /**
     * @brief Get peak level
     *
     * @param samples Audio samples
     * @return Peak level (0.0-1.0)
     */
    float GetPeakLevel(const std::vector<float>& samples);

    /**
     * @brief Apply pitch shift
     *
     * @param samples Input samples
     * @param pitch_factor Pitch adjustment (0.5-2.0)
     * @return Pitch-shifted samples
     */
    std::vector<float> ApplyPitchShift(const std::vector<float>& samples,
                                       float pitch_factor);

    /**
     * @brief Apply speed change (time stretch)
     *
     * @param samples Input samples
     * @param speed_factor Speed adjustment (0.5-2.0)
     * @return Time-stretched samples
     */
    std::vector<float> ApplySpeedChange(const std::vector<float>& samples,
                                        float speed_factor);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace jp_edge_tts

#endif // JP_EDGE_TTS_AUDIO_PROCESSOR_H