#ifdef NDEBUG
#undef NDEBUG
#endif
#include "lua/lua_object_registry.h"
#include "lua/lua_lifetime_observer.h"
#include <cassert>
#include <atomic>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

struct Object { int value; };
struct ObservedObject {
	~ObservedObject() { LuaAPI::LifetimeObserver<ObservedObject>::destroyed(this); }
};

int main() {
	using Registry = LuaAPI::ObjectRegistry<Object>;
	Registry::Handle survivingRegistry;
	std::optional<Object> storage;
	{
		Registry registry;
		assert(registry.watch(nullptr).get() == nullptr);
		storage.emplace(1);
		Object* const address = &*storage;
		auto first = registry.watch(address);
		auto alias = first;
		auto independentlyWatched = registry.watch(address);
		auto moved = std::move(first);
		assert(!first.get() && alias.get() == address && moved.get() == address && independentlyWatched.get() == address);
		registry.invalidate(address);
		registry.invalidate(address);
		assert(!alias.get() && !moved.get() && !independentlyWatched.get());
		storage.reset();
		storage.emplace(2);
		assert(&*storage == address);
		auto replacement = registry.watch(&*storage);
		assert(!alias.get() && replacement.get()->value == 2);
		registry.invalidateAll();
		assert(!replacement.get());
		replacement = registry.watch(&*storage);
		assert(replacement.get()->value == 2 && !alias.get());
		survivingRegistry = replacement;
		assert(survivingRegistry.get() == &*storage);
	}
	assert(survivingRegistry.get() == nullptr);
	survivingRegistry = {};

	// The object stays alive throughout; this checks registry bookkeeping,
	// not concurrent dereferencing during object destruction.
	Registry concurrent;
	Object value { 42 };
	std::vector<std::thread> threads;
	for (int i = 0; i < 4; ++i) {
		threads.emplace_back([&] {
			for (int n = 0; n < 10000; ++n) {
				auto handle = concurrent.watch(&value);
				auto copy = handle;
				Object* resolved = copy.get();
				assert(resolved == nullptr || resolved == &value);
				if ((n % 7) == 0) concurrent.invalidate(&value);
			}
		});
	}
	for (auto& thread : threads) thread.join();
	assert(!Registry::hasWatchedObjects());

	// Both engines see destruction, and reusing an address cannot revive a
	// previously issued handle. Destroying one observer must not detach another.
	using ObservedRegistry = LuaAPI::ObjectRegistry<ObservedObject>;
	ObservedRegistry a, b;
	LuaAPI::LifetimeObserver<ObservedObject> observerA(a);
	std::optional<ObservedObject> observed;
	observed.emplace();
	auto oldA = a.watch(&*observed);
	ObservedRegistry::Handle oldB;
	{
		LuaAPI::LifetimeObserver<ObservedObject> observerB(b);
		oldB = b.watch(&*observed);
		observed.reset();
		assert(!oldA.get() && !oldB.get());
		observed.emplace();
		oldA = a.watch(&*observed);
		oldB = b.watch(&*observed);
	}
	assert(!oldB.get());
	observed.reset();
	assert(!oldA.get() && !oldB.get());

	// Notifications may arrive from a worker while an engine unregisters.
	std::atomic<bool> running { true };
	ObservedObject workerObject;
	std::thread notifier([&] {
		while (running.load(std::memory_order_relaxed)) {
			LuaAPI::LifetimeObserver<ObservedObject>::destroyed(&workerObject);
		}
	});
	for (int i = 0; i < 1000; ++i) {
		LuaAPI::LifetimeObserver<ObservedObject> temporary(b);
		auto handle = b.watch(&workerObject);
	}
	running.store(false, std::memory_order_relaxed);
	notifier.join();
	assert(!ObservedRegistry::hasWatchedObjects());
}
