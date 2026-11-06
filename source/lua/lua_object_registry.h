//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_LUA_OBJECT_REGISTRY_H_
#define RME_LUA_OBJECT_REGISTRY_H_

#include <cstddef>
#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace LuaAPI {

// The registry and independently retained Lua handles can outlive one another;
// they share only this bookkeeping state, never ownership of the map object.
// Registry bookkeeping is synchronized. A returned raw pointer does not hold
// an object alive: callers must serialize dereference with object destruction.
template <typename T>
class ObjectRegistry {
	using Token = uint64_t;
	inline static std::atomic_size_t watchedObjects { 0 };
	struct Entry {
		T* object;
		std::size_t references;
	};
	struct State {
		std::mutex mutex;
		Token nextToken = 0;
		std::unordered_map<T*, Token> byObject;
		std::unordered_map<Token, Entry> entries;

		void release(Token token) noexcept {
			std::scoped_lock lock(mutex);
			const auto it = entries.find(token);
			if (it == entries.end() || --it->second.references != 0) return;
			if (it->second.object) {
				byObject.erase(it->second.object);
				watchedObjects.fetch_sub(1, std::memory_order_relaxed);
			}
			entries.erase(it);
		}
	};

public:
	class Handle {
	public:
		Handle() = default;
		Handle(const Handle& other) : state(other.state), token(other.token) {
			if (token) {
				std::scoped_lock lock(state->mutex);
				auto& count = state->entries.at(token).references;
				if (count == std::numeric_limits<std::size_t>::max()) {
					throw std::overflow_error("Lua object reference count exhausted");
				}
				++count;
			}
		}
		Handle(Handle&& other) noexcept : state(std::move(other.state)), token(std::exchange(other.token, 0)) {}
		Handle& operator=(Handle other) noexcept {
			swap(other);
			return *this;
		}
		~Handle() {
			if (token) state->release(token);
		}
		void swap(Handle& other) noexcept {
			state.swap(other.state);
			std::swap(token, other.token);
		}
		[[nodiscard]] T* get() const noexcept {
			if (!token) return nullptr;
			std::scoped_lock lock(state->mutex);
			const auto it = state->entries.find(token);
			return it == state->entries.end() ? nullptr : it->second.object;
		}

	private:
		friend class ObjectRegistry;
		Handle(std::shared_ptr<State> state, Token token) : state(std::move(state)), token(token) {}
		std::shared_ptr<State> state;
		Token token = 0;
	};

	ObjectRegistry() : state(std::make_shared<State>()) {}
	ObjectRegistry(const ObjectRegistry&) = delete;
	ObjectRegistry& operator=(const ObjectRegistry&) = delete;
	ObjectRegistry(ObjectRegistry&&) = delete;
	ObjectRegistry& operator=(ObjectRegistry&&) = delete;
	~ObjectRegistry() {
		// Handles surviving their registry remain valid userdata but resolve to null.
		invalidateAll();
	}

	[[nodiscard]] Handle watch(T* object) {
		if (!object) return {};
		std::scoped_lock lock(state->mutex);
		if (const auto it = state->byObject.find(object); it != state->byObject.end()) {
			auto& count = state->entries.at(it->second).references;
			if (count == std::numeric_limits<std::size_t>::max()) {
				throw std::overflow_error("Lua object reference count exhausted");
			}
			++count;
			return Handle(state, it->second);
		}
		if (state->nextToken == std::numeric_limits<Token>::max()) {
			throw std::overflow_error("Lua object identity space exhausted");
		}
		const Token token = ++state->nextToken;
		state->entries.emplace(token, Entry { object, 1 });
		try {
			state->byObject.emplace(object, token);
		} catch (...) {
			state->entries.erase(token);
			throw;
		}
		watchedObjects.fetch_add(1, std::memory_order_release);
		return Handle(state, token);
	}

	void invalidate(T* object) noexcept {
		std::scoped_lock lock(state->mutex);
		const auto it = state->byObject.find(object);
		if (it == state->byObject.end()) return;
		state->entries.at(it->second).object = nullptr;
		state->byObject.erase(it);
		watchedObjects.fetch_sub(1, std::memory_order_relaxed);
	}

	void invalidateAll() noexcept {
		std::scoped_lock lock(state->mutex);
		for (auto& [token, entry] : state->entries) entry.object = nullptr;
		watchedObjects.fetch_sub(state->byObject.size(), std::memory_order_relaxed);
		state->byObject.clear();
	}

	[[nodiscard]] static bool hasWatchedObjects() noexcept {
		return watchedObjects.load(std::memory_order_acquire) != 0;
	}

private:
	std::shared_ptr<State> state;
};

} // namespace LuaAPI

#endif
