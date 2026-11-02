// A small, thread-safe cache whose entries expire after some time.
//
// OOP ideas used here:
//   * Class TEMPLATE  - Cache<K, V> works for ANY key type K and value type V.
//                       The same code gives us Cache<std::string, ToolResult> today and
//                       Cache<int, Quiz> tomorrow. (This is "generic programming".)
//   * Encapsulation   - the map and the mutex are private. Users only see get() and put().
//   * RAII            - std::lock_guard locks the mutex in its constructor and unlocks it
//                       in its destructor, even if an exception is thrown.
//   * std::optional   - get() returns "a value or nothing" instead of a magic null.
//
// Why a mutex? Our server handles several requests at the same time (threads),
// and two threads must not change the same std::map at once.
#ifndef VELORA_UTIL_CACHE_H
#define VELORA_UTIL_CACHE_H

#include <chrono>
#include <cstddef>
#include <map>
#include <mutex>
#include <optional>

template <typename K, typename V>
class Cache {
public:
    using Clock = std::chrono::steady_clock;

    Cache(std::size_t maxEntries, std::chrono::seconds timeToLive)
        : maxEntries_(maxEntries), ttl_(timeToLive) {}

    // Returns the stored value, or an empty optional if missing/expired.
    std::optional<V> get(const K& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = items_.find(key);
        if (it == items_.end()) return std::nullopt;
        if (Clock::now() - it->second.storedAt > ttl_) {
            items_.erase(it);
            return std::nullopt;
        }
        return it->second.value;
    }

    void put(const K& key, V value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (items_.size() >= maxEntries_ && items_.find(key) == items_.end())
            evictOldest();
        items_[key] = Entry{std::move(value), Clock::now()};
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return items_.size();
    }

private:
    struct Entry {
        V value;
        Clock::time_point storedAt;
    };

    void evictOldest() {  // caller already holds the lock
        auto oldest = items_.begin();
        for (auto it = items_.begin(); it != items_.end(); ++it)
            if (it->second.storedAt < oldest->second.storedAt) oldest = it;
        if (oldest != items_.end()) items_.erase(oldest);
    }

    std::size_t maxEntries_;
    std::chrono::seconds ttl_;
    std::map<K, Entry> items_;
    mutable std::mutex mutex_;  // "mutable": size() is const but still needs to lock
};

#endif  // VELORA_UTIL_CACHE_H
