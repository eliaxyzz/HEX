/**
 * @file resource_cache.h
 * @brief Generic cache: every resource is built exactly once.
 *
 * @note SFML-free and free of graphics dependencies. Loading each resource once
 * is caching logic rather than rendering, so it lives here and is verifiable
 * without opening a window or touching a real file.
 */

#ifndef RESOURCE_CACHE_H
#define RESOURCE_CACHE_H

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace hexassets {

    /**
     * @brief Container that builds each resource on first use and reuses it after.
     *
     * Resources are held through unique_ptr for a specific reason: a returned
     * pointer must stay valid as the map grows and rehashes. Storing values inline
     * would let a later get() of a different key invalidate pointers already handed
     * out.
     *
     * @tparam T Resource type.
     */
    template <typename T>
    class ResourceCache {
    public:
        /** @brief Builds the resource identified by a key. */
        using Loader = std::function<std::unique_ptr<T>(const std::string& key)>;

        /** @brief Builds the cache around a loader function. */
        explicit ResourceCache(Loader loader) : loader(std::move(loader)) {}

        /**
         * @brief Returns the resource, loading it on first request only.
         * @return Pointer to the resource, or null if loading failed.
         * @note Failures are memoised too, so a missing file is not looked up again
         * on every frame.
         */
        [[nodiscard]] T* get(const std::string& key) {
            const auto found = entries.find(key);
            if (found != entries.end()) return found->second.get();

            std::unique_ptr<T> loaded = loader ? loader(key) : nullptr;
            T* raw = loaded.get();
            entries.emplace(key, std::move(loaded));
            return raw;
        }

        /** @brief Tests whether a key has been requested at least once. */
        [[nodiscard]] bool contains(const std::string& key) const {
            return entries.contains(key);
        }

        /** @brief Returns the number of keys requested, successful or not. */
        [[nodiscard]] std::size_t size() const { return entries.size(); }

        /** @brief Empties the cache.
         * @warning Pointers already handed out become dangling. */
        void clear() { entries.clear(); }

    private:
        /** @brief Function building a resource from its key. */
        Loader loader;

        /**
         * @brief Resources already built.
         * @note A null value means "tried, unavailable", which is what stops a failed
         * load from being retried every frame.
         */
        std::unordered_map<std::string, std::unique_ptr<T>> entries;
    };
}

#endif //RESOURCE_CACHE_H
