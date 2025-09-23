/**
 * @file cache_manager.cpp
 * @brief Implementation of TTS result caching system
 * @author D Everett Hinton
 * @date 2025
 *
 * @copyright MIT License
 */

#include "jp_edge_tts/core/cache_manager.h"
#include "jp_edge_tts/utils/string_utils.h"
#include <chrono>
#include <algorithm>
#include <iostream>

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
        std::vector<float> audio_data;
        int sample_rate;
        std::chrono::steady_clock::time_point last_access;
        size_t access_count;
        size_t memory_size;
    };

    std::unordered_map<std::string, CacheEntry> cache;
    std::list<std::string> lru_list;  // Front = most recently used
    std::unordered_map<std::string, std::list<std::string>::iterator> lru_map;
    
    size_t max_entries;
    size_t max_memory_mb;
    size_t current_memory;
    bool enabled;
    mutable std::mutex mutex;
    
    // Statistics
    size_t hits = 0;
    size_t misses = 0;
    size_t evictions = 0;

    Impl() : max_entries(100), max_memory_mb(500), current_memory(0), enabled(true) {}

    std::string GenerateCacheKey(
        const std::string& text,
        const std::string& voice_id,
        float speed,
        float pitch,
        const std::string& phonemes) {
        
        // Create a unique key from all parameters
        std::stringstream ss;
        ss << text << "|" << voice_id << "|"
           << std::fixed << std::setprecision(2) << speed << "|"
           << std::fixed << std::setprecision(2) << pitch;
        
        // Include phonemes if provided (for consistency)
        if (!phonemes.empty()) {
            ss << "|" << phonemes;
        }
        
        // Hash the combined string for shorter keys
        size_t hash = StringUtils::Hash(ss.str());
        return std::to_string(hash);
    }

    void TouchEntry(const std::string& key) {
        // Move to front of LRU list
        auto it = lru_map.find(key);
        if (it != lru_map.end()) {
            lru_list.erase(it->second);
        }
        lru_list.push_front(key);
        lru_map[key] = lru_list.begin();
        
        // Update access time and count
        if (cache.find(key) != cache.end()) {
            cache[key].last_access = std::chrono::steady_clock::now();
            cache[key].access_count++;
        }
    }

    void EvictLRU() {
        // Evict least recently used entry
        if (!lru_list.empty()) {
            std::string key_to_evict = lru_list.back();
            lru_list.pop_back();
            lru_map.erase(key_to_evict);
            
            // Free memory
            auto it = cache.find(key_to_evict);
            if (it != cache.end()) {
                current_memory -= it->second.memory_size;
                cache.erase(it);
                evictions++;
            }
        }
    }

    void EnforceMemoryLimit() {
        // Convert MB to bytes for comparison
        size_t max_bytes = max_memory_mb * 1024 * 1024;
        
        // Evict entries until under memory limit
        while (current_memory > max_bytes && !cache.empty()) {
            EvictLRU();
        }
    }

    void EnforceEntryLimit() {
        // Evict entries until under entry count limit
        while (cache.size() >= max_entries && !cache.empty()) {
            EvictLRU();
        }
    }

    size_t CalculateMemorySize(const std::vector<float>& audio_data) {
        // Calculate memory footprint
        return audio_data.size() * sizeof(float) + sizeof(CacheEntry);
    }
};

// ==========================================
// Public Interface Implementation
// ==========================================

CacheManager::CacheManager() : pImpl(std::make_unique<Impl>()) {}
CacheManager::~CacheManager() = default;
CacheManager::CacheManager(CacheManager&&) noexcept = default;
CacheManager& CacheManager::operator=(CacheManager&&) noexcept = default;

void CacheManager::SetMaxEntries(size_t max_entries) {
    std::lock_guard<std::mutex> lock(pImpl->mutex);
    pImpl->max_entries = max_entries;
    pImpl->EnforceEntryLimit();
}

void CacheManager::SetMaxMemoryMB(size_t max_memory_mb) {
    std::lock_guard<std::mutex> lock(pImpl->mutex);
    pImpl->max_memory_mb = max_memory_mb;
    pImpl->EnforceMemoryLimit();
}

void CacheManager::SetEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(pImpl->mutex);
    pImpl->enabled = enabled;
    
    // Clear cache if disabling
    if (!enabled) {
        Clear();
    }
}

bool CacheManager::Put(
    const std::string& text,
    const std::string& voice_id,
    float speed,
    float pitch,
    const std::string& phonemes,
    const std::vector<float>& audio_data,
    int sample_rate) {
    
    std::lock_guard<std::mutex> lock(pImpl->mutex);
    
    // Skip if caching disabled
    if (!pImpl->enabled) {
        return false;
    }
    
    // Generate cache key
    std::string key = pImpl->GenerateCacheKey(text, voice_id, speed, pitch, phonemes);
    
    // Check if already exists
    if (pImpl->cache.find(key) != pImpl->cache.end()) {
        pImpl->TouchEntry(key);
        return true;
    }
    
    // Calculate memory size
    size_t memory_size = pImpl->CalculateMemorySize(audio_data);
    
    // Enforce limits before adding
    pImpl->EnforceEntryLimit();
    pImpl->current_memory += memory_size;
    pImpl->EnforceMemoryLimit();
    
    // Create cache entry
    Impl::CacheEntry entry;
    entry.key = key;
    entry.audio_data = audio_data;
    entry.sample_rate = sample_rate;
    entry.last_access = std::chrono::steady_clock::now();
    entry.access_count = 1;
    entry.memory_size = memory_size;
    
    // Add to cache
    pImpl->cache[key] = std::move(entry);
    pImpl->TouchEntry(key);
    
    return true;
}

std::optional<CachedResult> CacheManager::Get(
    const std::string& text,
    const std::string& voice_id,
    float speed,
    float pitch,
    const std::string& phonemes) {
    
    std::lock_guard<std::mutex> lock(pImpl->mutex);
    
    // Skip if caching disabled
    if (!pImpl->enabled) {
        pImpl->misses++;
        return std::nullopt;
    }
    
    // Generate cache key
    std::string key = pImpl->GenerateCacheKey(text, voice_id, speed, pitch, phonemes);
    
    // Look up in cache
    auto it = pImpl->cache.find(key);
    if (it != pImpl->cache.end()) {
        // Cache hit - update LRU and stats
        pImpl->TouchEntry(key);
        pImpl->hits++;
        
        // Return cached result
        CachedResult result;
        result.audio_data = it->second.audio_data;
        result.sample_rate = it->second.sample_rate;
        result.from_cache = true;
        
        return result;
    }
    
    // Cache miss
    pImpl->misses++;
    return std::nullopt;
}

bool CacheManager::Contains(
    const std::string& text,
    const std::string& voice_id,
    float speed,
    float pitch,
    const std::string& phonemes) const {
    
    std::lock_guard<std::mutex> lock(pImpl->mutex);
    
    if (!pImpl->enabled) {
        return false;
    }
    
    std::string key = pImpl->GenerateCacheKey(text, voice_id, speed, pitch, phonemes);
    return pImpl->cache.find(key) != pImpl->cache.end();
}

void CacheManager::Clear() {
    std::lock_guard<std::mutex> lock(pImpl->mutex);
    
    pImpl->cache.clear();
    pImpl->lru_list.clear();
    pImpl->lru_map.clear();
    pImpl->current_memory = 0;
    
    // Don't reset statistics on clear
}

size_t CacheManager::GetEntryCount() const {
    std::lock_guard<std::mutex> lock(pImpl->mutex);
    return pImpl->cache.size();
}

size_t CacheManager::GetMemoryUsageMB() const {
    std::lock_guard<std::mutex> lock(pImpl->mutex);
    return pImpl->current_memory / (1024 * 1024);
}

CacheStats CacheManager::GetStats() const {
    std::lock_guard<std::mutex> lock(pImpl->mutex);
    
    CacheStats stats;
    stats.hits = pImpl->hits;
    stats.misses = pImpl->misses;
    stats.evictions = pImpl->evictions;
    stats.entry_count = pImpl->cache.size();
    stats.memory_usage_mb = GetMemoryUsageMB();
    
    // Calculate hit ratio
    size_t total = stats.hits + stats.misses;
    stats.hit_ratio = (total > 0) ? static_cast<float>(stats.hits) / total : 0.0f;
    
    return stats;
}

void CacheManager::ResetStats() {
    std::lock_guard<std::mutex> lock(pImpl->mutex);
    
    pImpl->hits = 0;
    pImpl->misses = 0;
    pImpl->evictions = 0;
}

void CacheManager::PruneOldEntries(std::chrono::minutes max_age) {
    std::lock_guard<std::mutex> lock(pImpl->mutex);
    
    auto now = std::chrono::steady_clock::now();
    std::vector<std::string> to_remove;
    
    // Find entries older than max_age
    for (const auto& [key, entry] : pImpl->cache) {
        auto age = std::chrono::duration_cast<std::chrono::minutes>(
            now - entry.last_access
        );
        
        if (age > max_age) {
            to_remove.push_back(key);
        }
    }
    
    // Remove old entries
    for (const auto& key : to_remove) {
        auto it = pImpl->cache.find(key);
        if (it != pImpl->cache.end()) {
            // Update memory tracking
            pImpl->current_memory -= it->second.memory_size;
            
            // Remove from LRU structures
            auto lru_it = pImpl->lru_map.find(key);
            if (lru_it != pImpl->lru_map.end()) {
                pImpl->lru_list.erase(lru_it->second);
                pImpl->lru_map.erase(lru_it);
            }
            
            // Remove from cache
            pImpl->cache.erase(it);
            pImpl->evictions++;
        }
    }
}

} // namespace jp_edge_tts