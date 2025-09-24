/**
 * @file cache_manager.cpp
 * @brief Implementation of TTS result caching system
 * @author D Everett Hinton
 * @date 2025
 *
 * @copyright MIT License
 */

#include "jp_edge_tts/core/cache_manager.h"
#include <chrono>
#include <algorithm>
#include <iostream>
#include <unordered_map>
#include <list>
#include <mutex>
#include <vector>

// For hashing audio data
#include <cstring>
#include <sstream>
#include <iomanip>

namespace jp_edge_tts {

// ==========================================
// Private Implementation
// ==========================================

class CacheManager::Impl {
public:
    struct CacheEntry {
        std::string key;
        TTSResult result;
        std::chrono::steady_clock::time_point created;
        std::chrono::steady_clock::time_point last_access;
        size_t access_count;
        size_t memory_size;
    };

    std::unordered_map<std::string, CacheEntry> cache;
    std::list<std::string> lru_list;  // Front = most recently used
    std::unordered_map<std::string, std::list<std::string>::iterator> lru_map;

    size_t max_size_bytes;
    size_t current_size_bytes;
    int ttl_seconds;

    mutable std::mutex cache_mutex;

    // Statistics
    size_t hits = 0;
    size_t misses = 0;
    size_t evictions = 0;

    Impl(size_t max_size_bytes)
        : max_size_bytes(max_size_bytes)
        , current_size_bytes(0)
        , ttl_seconds(0) // 0 = no expiry
    {}

    void MoveLRU(const std::string& key) {
        auto lru_it = lru_map.find(key);
        if (lru_it != lru_map.end()) {
            // Remove from current position
            lru_list.erase(lru_it->second);
        }

        // Add to front
        lru_list.push_front(key);
        lru_map[key] = lru_list.begin();
    }

    void RemoveLRU(const std::string& key) {
        auto lru_it = lru_map.find(key);
        if (lru_it != lru_map.end()) {
            lru_list.erase(lru_it->second);
            lru_map.erase(lru_it);
        }
    }

    void EvictLRU() {
        while (!lru_list.empty() && current_size_bytes > max_size_bytes) {
            std::string oldest_key = lru_list.back();
            lru_list.pop_back();

            auto cache_it = cache.find(oldest_key);
            if (cache_it != cache.end()) {
                current_size_bytes -= cache_it->second.memory_size;
                cache.erase(cache_it);
                evictions++;
            }

            lru_map.erase(oldest_key);
        }
    }

    size_t CalculateMemorySize(const TTSResult& result) {
        size_t size = sizeof(TTSResult);
        size += result.audio.samples.size() * sizeof(float);
        size += result.phonemes.size() * sizeof(PhonemeInfo);
        size += result.tokens.size() * sizeof(TokenInfo);
        size += result.error_message.size();
        return size;
    }

    bool IsExpired(const CacheEntry& entry) const {
        if (ttl_seconds <= 0) return false; // No expiry

        auto now = std::chrono::steady_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - entry.created);
        return age.count() > ttl_seconds;
    }
};

// ==========================================
// Public Interface Implementation
// ==========================================

CacheManager::CacheManager(size_t max_size_bytes)
    : pImpl(std::make_unique<Impl>(max_size_bytes)) {}

CacheManager::~CacheManager() = default;
CacheManager::CacheManager(CacheManager&&) noexcept = default;
CacheManager& CacheManager::operator=(CacheManager&&) noexcept = default;

std::optional<TTSResult> CacheManager::Get(const std::string& key) {
    std::lock_guard<std::mutex> lock(pImpl->cache_mutex);

    auto it = pImpl->cache.find(key);
    if (it != pImpl->cache.end()) {
        // Check if expired
        if (pImpl->IsExpired(it->second)) {
            // Remove expired entry
            pImpl->current_size_bytes -= it->second.memory_size;
            pImpl->RemoveLRU(key);
            pImpl->cache.erase(it);
            pImpl->misses++;
            return std::nullopt;
        }

        // Update access info
        it->second.last_access = std::chrono::steady_clock::now();
        it->second.access_count++;

        // Move to front of LRU
        pImpl->MoveLRU(key);

        pImpl->hits++;
        return it->second.result;
    }

    pImpl->misses++;
    return std::nullopt;
}

void CacheManager::Put(const std::string& key, const TTSResult& result) {
    std::lock_guard<std::mutex> lock(pImpl->cache_mutex);

    // Calculate memory size
    size_t memory_size = pImpl->CalculateMemorySize(result);

    // Check if key already exists
    auto it = pImpl->cache.find(key);
    if (it != pImpl->cache.end()) {
        // Update existing entry
        pImpl->current_size_bytes -= it->second.memory_size;
        it->second.result = result;
        it->second.memory_size = memory_size;
        it->second.last_access = std::chrono::steady_clock::now();
        it->second.access_count++;
        pImpl->current_size_bytes += memory_size;

        // Move to front of LRU
        pImpl->MoveLRU(key);
    } else {
        // Create new entry
        Impl::CacheEntry entry;
        entry.key = key;
        entry.result = result;
        entry.created = std::chrono::steady_clock::now();
        entry.last_access = entry.created;
        entry.access_count = 1;
        entry.memory_size = memory_size;

        pImpl->cache[key] = std::move(entry);
        pImpl->current_size_bytes += memory_size;

        // Add to LRU
        pImpl->MoveLRU(key);
    }

    // Evict if over size limit
    pImpl->EvictLRU();
}

bool CacheManager::Has(const std::string& key) const {
    std::lock_guard<std::mutex> lock(pImpl->cache_mutex);

    auto it = pImpl->cache.find(key);
    if (it != pImpl->cache.end()) {
        return !pImpl->IsExpired(it->second);
    }

    return false;
}

bool CacheManager::Remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(pImpl->cache_mutex);

    auto it = pImpl->cache.find(key);
    if (it != pImpl->cache.end()) {
        pImpl->current_size_bytes -= it->second.memory_size;
        pImpl->RemoveLRU(key);
        pImpl->cache.erase(it);
        return true;
    }

    return false;
}

void CacheManager::Clear() {
    std::lock_guard<std::mutex> lock(pImpl->cache_mutex);

    pImpl->cache.clear();
    pImpl->lru_list.clear();
    pImpl->lru_map.clear();
    pImpl->current_size_bytes = 0;
}

CacheManager::CacheStats CacheManager::GetStats() const {
    std::lock_guard<std::mutex> lock(pImpl->cache_mutex);

    CacheStats stats;
    stats.total_entries = pImpl->cache.size();
    stats.total_size_bytes = pImpl->current_size_bytes;
    stats.hit_count = pImpl->hits;
    stats.miss_count = pImpl->misses;
    stats.hit_rate = (pImpl->hits + pImpl->misses > 0) ?
        static_cast<float>(pImpl->hits) / (pImpl->hits + pImpl->misses) : 0.0f;
    stats.eviction_count = pImpl->evictions;

    return stats;
}

void CacheManager::ResetStats() {
    std::lock_guard<std::mutex> lock(pImpl->cache_mutex);

    pImpl->hits = 0;
    pImpl->misses = 0;
    pImpl->evictions = 0;
}

void CacheManager::SetMaxSize(size_t max_size_bytes) {
    std::lock_guard<std::mutex> lock(pImpl->cache_mutex);

    pImpl->max_size_bytes = max_size_bytes;
    pImpl->EvictLRU();
}

size_t CacheManager::GetCurrentSize() const {
    std::lock_guard<std::mutex> lock(pImpl->cache_mutex);
    return pImpl->current_size_bytes;
}

size_t CacheManager::GetEntryCount() const {
    std::lock_guard<std::mutex> lock(pImpl->cache_mutex);
    return pImpl->cache.size();
}

void CacheManager::SetTTL(int ttl_seconds) {
    std::lock_guard<std::mutex> lock(pImpl->cache_mutex);
    pImpl->ttl_seconds = ttl_seconds;
}

size_t CacheManager::CleanExpired() {
    std::lock_guard<std::mutex> lock(pImpl->cache_mutex);

    if (pImpl->ttl_seconds <= 0) return 0; // No expiry

    size_t removed_count = 0;
    auto now = std::chrono::steady_clock::now();

    for (auto it = pImpl->cache.begin(); it != pImpl->cache.end();) {
        if (pImpl->IsExpired(it->second)) {
            pImpl->current_size_bytes -= it->second.memory_size;
            pImpl->RemoveLRU(it->first);
            it = pImpl->cache.erase(it);
            removed_count++;
        } else {
            ++it;
        }
    }

    return removed_count;
}

int CacheManager::LoadFromDisk(const std::string& cache_dir) {
    // Basic implementation - could be enhanced to actually load from disk
    // For now, just return 0 as no entries were loaded
    return 0;
}

int CacheManager::SaveToDisk(const std::string& cache_dir) const {
    // Basic implementation - could be enhanced to actually save to disk
    // For now, just return the current entry count
    return static_cast<int>(GetEntryCount());
}

} // namespace jp_edge_tts