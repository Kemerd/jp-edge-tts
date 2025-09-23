/**
 * @file cache_manager.h
 * @brief LRU cache management for TTS results
 * D Everett Hinton
 * @date 2025
 *
 * @copyright MIT License
 */

#ifndef JP_EDGE_TTS_CACHE_MANAGER_H
#define JP_EDGE_TTS_CACHE_MANAGER_H

#include "jp_edge_tts/types.h"
#include <memory>
#include <string>
#include <optional>

namespace jp_edge_tts {

/**
 * @class CacheManager
 * @brief Manages caching of TTS synthesis results
 *
 * @details Implements an LRU (Least Recently Used) cache for
 * storing synthesized audio to avoid redundant processing.
 */
class CacheManager {
public:
    /**
     * @brief Constructor
     * @param max_size_bytes Maximum cache size in bytes
     */
    explicit CacheManager(size_t max_size_bytes = 100 * 1024 * 1024);

    /**
     * @brief Destructor
     */
    ~CacheManager();

    // Disable copy, enable move
    CacheManager(const CacheManager&) = delete;
    CacheManager& operator=(const CacheManager&) = delete;
    CacheManager(CacheManager&&) noexcept;
    CacheManager& operator=(CacheManager&&) noexcept;

    /**
     * @brief Get cached result
     *
     * @param key Cache key
     * @return TTS result if found in cache
     */
    std::optional<TTSResult> Get(const std::string& key);

    /**
     * @brief Store result in cache
     *
     * @param key Cache key
     * @param result TTS result to cache
     */
    void Put(const std::string& key, const TTSResult& result);

    /**
     * @brief Check if key exists in cache
     *
     * @param key Cache key
     * @return true if key exists
     */
    bool Has(const std::string& key) const;

    /**
     * @brief Remove entry from cache
     *
     * @param key Cache key
     * @return true if entry was removed
     */
    bool Remove(const std::string& key);

    /**
     * @brief Clear all cache entries
     */
    void Clear();

    /**
     * @brief Get cache statistics
     */
    struct CacheStats {
        size_t total_entries;
        size_t total_size_bytes;
        size_t hit_count;
        size_t miss_count;
        float hit_rate;
        size_t eviction_count;
    };
    CacheStats GetStats() const;

    /**
     * @brief Reset statistics
     */
    void ResetStats();

    /**
     * @brief Set maximum cache size
     * @param max_size_bytes Maximum size in bytes
     */
    void SetMaxSize(size_t max_size_bytes);

    /**
     * @brief Get current cache size
     * @return Size in bytes
     */
    size_t GetCurrentSize() const;

    /**
     * @brief Get number of cached entries
     * @return Entry count
     */
    size_t GetEntryCount() const;

    /**
     * @brief Set cache TTL (time to live)
     * @param ttl_seconds TTL in seconds (0 = no expiry)
     */
    void SetTTL(int ttl_seconds);

    /**
     * @brief Clean expired entries
     * @return Number of entries removed
     */
    size_t CleanExpired();

    /**
     * @brief Preload cache from directory
     *
     * @param cache_dir Directory containing cached results
     * @return Number of entries loaded
     */
    int LoadFromDisk(const std::string& cache_dir);

    /**
     * @brief Save cache to directory
     *
     * @param cache_dir Output directory
     * @return Number of entries saved
     */
    int SaveToDisk(const std::string& cache_dir) const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace jp_edge_tts

#endif // JP_EDGE_TTS_CACHE_MANAGER_H