//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_LUA_LIFETIME_OBSERVER_H_
#define RME_LUA_LIFETIME_OBSERVER_H_

#include "lua/lua_object_registry.h"

#include <atomic>
#include <mutex>

namespace LuaAPI {

// Registrations belong to engines, not to the observed objects. Destruction
// only invalidates identity; access to the objects themselves must still be
// serialized with destruction by the caller.
template <typename T>
class LifetimeObserver {
	struct Hub {
		std::mutex mutex;
		LifetimeObserver* first = nullptr;
	};

	static Hub& hub() {
		// The first registration constructs the hub before its owner finishes
		// construction, so it remains alive through that owner's destruction.
		static Hub instance;
		return instance;
	}

	inline static std::atomic<bool> active { false };

public:
	explicit LifetimeObserver(ObjectRegistry<T>& registry) : registry(registry) {
		auto& state = hub();
		std::scoped_lock lock(state.mutex);
		next = state.first;
		state.first = this;
		active.store(true, std::memory_order_release);
	}

	LifetimeObserver(const LifetimeObserver&) = delete;
	LifetimeObserver& operator=(const LifetimeObserver&) = delete;

	~LifetimeObserver() {
		auto& state = hub();
		std::scoped_lock lock(state.mutex);
		// Handles must not remain usable after this registry stops receiving
		// destruction notifications.
		registry.invalidateAll();
		auto** link = &state.first;
		while (*link && *link != this) link = &(*link)->next;
		if (*link) *link = next;
		active.store(state.first != nullptr, std::memory_order_release);
	}

	static void destroyed(T* object) noexcept {
		// No mutex on the common path where no Lua handle watches this type.
		// Threads issuing notifications must be joined before process teardown.
		if (!active.load(std::memory_order_acquire) || !ObjectRegistry<T>::hasWatchedObjects()) return;
		auto& state = hub();
		std::scoped_lock lock(state.mutex);
		for (auto* observer = state.first; observer; observer = observer->next) {
			observer->registry.invalidate(object);
		}
	}

private:
	ObjectRegistry<T>& registry;
	LifetimeObserver* next = nullptr;
};

} // namespace LuaAPI

#endif
