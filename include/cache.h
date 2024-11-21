#ifndef MONITOR_CACHE_H
#define MONITOR_CACHE_H

// General caching class to refrain from expensive reads

#include <chrono>

template <typename T>
class Cache {
 public:
  explicit Cache(std::chrono::milliseconds duration) : cacheDuration(duration) {}

  // Check if cache is valid (if the data is still fresh)
  [[nodiscard]] bool IsCacheValid() const {
    return std::chrono::steady_clock::now() - lastUpdate < cacheDuration;
  }

  // Update the cached value
  void UpdateCache(const T& value) {
    cachedValue = value;
    lastUpdate = std::chrono::steady_clock::now();
  }

  // Get the cached value
  const T& GetValue() const { return cachedValue; }

 private:
  std::chrono::milliseconds cacheDuration;
  std::chrono::steady_clock::time_point lastUpdate;
  T cachedValue;
};

#endif
