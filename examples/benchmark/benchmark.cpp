/**
 * @file benchmark.cpp
 * @brief Performance benchmarking utility for JP Edge TTS
 * @author D Everett Hinton
 * @date 2025
 *
 * @copyright MIT License
 */

#include "jp_edge_tts/core/tts_engine.h"
#include <chrono>
#include <iostream>
#include <vector>
#include <numeric>
#include <iomanip>

using namespace jp_edge_tts;

// Test phrases for benchmarking
const std::vector<std::string> TEST_PHRASES = {
    "こんにちは、今日はいい天気ですね。",
    "日本語の音声合成技術は進歩しています。",
    "これは性能テストのためのサンプル文章です。",
    "ベンチマークテストを実行中です。",
    "音声品質と処理速度のバランスが重要です。"
};

/**
 * @brief Simple timing utility
 */
class Timer {
public:
    void start() {
        start_time = std::chrono::high_resolution_clock::now();
    }

    double stop() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        return duration.count();
    }

private:
    std::chrono::high_resolution_clock::time_point start_time;
};

/**
 * @brief Run synchronous synthesis benchmark
 */
void benchmarkSync(TTSEngine& engine, const std::string& voice_id, int iterations) {
    std::cout << "\n=== Synchronous Synthesis Benchmark ===" << std::endl;
    std::cout << "Iterations: " << iterations << std::endl;
    std::cout << "Voice: " << voice_id << std::endl;

    std::vector<double> latencies;
    Timer timer;

    for (int i = 0; i < iterations; ++i) {
        const auto& text = TEST_PHRASES[i % TEST_PHRASES.size()];

        timer.start();
        auto result = engine.SynthesizeSimple(text, voice_id);
        double latency = timer.stop();

        if (result.IsSuccess()) {
            latencies.push_back(latency);
            std::cout << "." << std::flush;
        } else {
            std::cout << "E" << std::flush;
        }
    }

    std::cout << std::endl;

    if (!latencies.empty()) {
        double avg = std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();
        double min = *std::min_element(latencies.begin(), latencies.end());
        double max = *std::max_element(latencies.begin(), latencies.end());

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Average latency: " << avg << " ms" << std::endl;
        std::cout << "Min latency: " << min << " ms" << std::endl;
        std::cout << "Max latency: " << max << " ms" << std::endl;
        std::cout << "Throughput: " << (1000.0 / avg) << " requests/second" << std::endl;
    }
}

/**
 * @brief Run asynchronous synthesis benchmark
 */
void benchmarkAsync(TTSEngine& engine, const std::string& voice_id, int iterations) {
    std::cout << "\n=== Asynchronous Synthesis Benchmark ===" << std::endl;
    std::cout << "Iterations: " << iterations << std::endl;
    std::cout << "Voice: " << voice_id << std::endl;

    Timer timer;
    timer.start();

    // Submit all requests
    std::vector<std::future<TTSResult>> futures;
    for (int i = 0; i < iterations; ++i) {
        const auto& text = TEST_PHRASES[i % TEST_PHRASES.size()];

        TTSRequest request;
        request.text = text;
        request.voice_id = voice_id;

        futures.push_back(engine.SynthesizeAsync(request));
    }

    // Wait for all results
    int successful = 0;
    for (auto& future : futures) {
        auto result = future.get();
        if (result.IsSuccess()) {
            successful++;
            std::cout << "." << std::flush;
        } else {
            std::cout << "E" << std::flush;
        }
    }

    double total_time = timer.stop();

    std::cout << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Total time: " << total_time << " ms" << std::endl;
    std::cout << "Successful: " << successful << "/" << iterations << std::endl;
    std::cout << "Throughput: " << (successful * 1000.0 / total_time) << " requests/second" << std::endl;
}

/**
 * @brief Run cache performance test
 */
void benchmarkCache(TTSEngine& engine, const std::string& voice_id) {
    std::cout << "\n=== Cache Performance Benchmark ===" << std::endl;

    const std::string test_text = "キャッシュ性能テストです。";

    // Cold run
    Timer timer;
    timer.start();
    auto result1 = engine.SynthesizeSimple(test_text, voice_id);
    double cold_time = timer.stop();

    // Warm run (should hit cache)
    timer.start();
    auto result2 = engine.SynthesizeSimple(test_text, voice_id);
    double warm_time = timer.stop();

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Cold synthesis: " << cold_time << " ms" << std::endl;
    std::cout << "Cached synthesis: " << warm_time << " ms" << std::endl;
    std::cout << "Cache speedup: " << (cold_time / warm_time) << "x" << std::endl;

    // Cache statistics
    auto cache_stats = engine.GetCacheStats();
    std::cout << "Cache entries: " << cache_stats.total_entries << std::endl;
    std::cout << "Cache hit rate: " << (cache_stats.hit_rate * 100) << "%" << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "JP Edge TTS Performance Benchmark" << std::endl;
    std::cout << "================================" << std::endl;

    // Parse command line arguments
    int iterations = 10;
    std::string voice_id = "jf_alpha";

    if (argc > 1) {
        iterations = std::atoi(argv[1]);
    }
    if (argc > 2) {
        voice_id = argv[2];
    }

    // Initialize engine
    TTSConfig config;
    config.enable_cache = true;
    config.max_concurrent_requests = 4;

    auto engine = CreateTTSEngine(config);

    std::cout << "Initializing engine..." << std::endl;
    auto status = engine->Initialize();
    if (status != Status::OK) {
        std::cerr << "Failed to initialize TTS engine!" << std::endl;
        return 1;
    }

    // Warmup
    std::cout << "Warming up..." << std::endl;
    engine->SynthesizeSimple("ウォームアップ", voice_id);

    // Run benchmarks
    benchmarkSync(*engine, voice_id, iterations);
    benchmarkAsync(*engine, voice_id, iterations);
    benchmarkCache(*engine, voice_id);

    // Final stats
    auto perf_stats = engine->GetPerformanceStats();
    std::cout << "\n=== Overall Statistics ===" << std::endl;
    std::cout << "Total requests: " << perf_stats.total_requests << std::endl;
    std::cout << "Successful: " << perf_stats.successful_requests << std::endl;
    std::cout << "Failed: " << perf_stats.failed_requests << std::endl;
    std::cout << "Average latency: " << perf_stats.average_latency.count() << " ms" << std::endl;

    return 0;
}