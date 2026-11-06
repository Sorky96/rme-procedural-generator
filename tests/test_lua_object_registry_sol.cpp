#ifdef NDEBUG
#undef NDEBUG
#endif
#include "lua/lua_object_registry.h"
#include "lua/lua_sol_config.h"
#include <cassert>
#include <optional>

struct Object { int value; };
struct TraitProbeObject { int value; };
struct TraitProbeHandle { LuaAPI::ObjectRegistry<TraitProbeObject>::Handle handle; };

namespace sol {
	template <> struct unique_usertype_traits<TraitProbeHandle> {
		using type = TraitProbeObject;
		using actual_type = TraitProbeHandle;
		static constexpr bool value = true;
		static bool is_null(const actual_type& value) { return !value.handle.get(); }
		static type* get(const actual_type& value) { return value.handle.get(); }
	};
}

int main() {
	using Registry = LuaAPI::ObjectRegistry<Object>;
	using Handle = Registry::Handle;
	sol::state lua;
	lua.open_libraries(sol::lib::base, sol::lib::string);
	lua.new_usertype<Handle>("Handle", sol::no_constructor,
		"value", sol::property([](const Handle& handle) {
			const Object* object = handle.get();
			if (!object) throw sol::error("expired object");
			return object->value;
		}));
	std::optional<Handle> returnedHandle;
	lua["get_handle"] = [&returnedHandle]() -> std::optional<Handle> { return returnedHandle; };
	lua.script("assert(get_handle() == nil)");
	std::optional<Object> object;
	{
		Registry registry;
		object.emplace(12);
		lua["first"] = registry.watch(&*object);
		lua["second"] = registry.watch(&*object);
		returnedHandle = registry.watch(&*object);
		lua.script("assert(get_handle().value == 12)");
		lua.script("assert(first.value == 12 and second.value == 12)");
		registry.invalidate(&*object);
		lua.script("assert(not pcall(function() return get_handle().value end))");
		object.reset();
		object.emplace(34);
		lua["replacement"] = registry.watch(&*object);
		lua.script(R"(
			local ok, message = pcall(function() return first.value end)
			assert(not ok and message:find('expired object', 1, true))
			assert(replacement.value == 34)
			first = nil; second = nil; collectgarbage('collect')
			assert(replacement.value == 34)
		)");
	}
	lua.script(R"(
		local ok, message = pcall(function() return replacement.value end)
		assert(not ok and message:find('expired object', 1, true))
	)");

	// A smart-pointer trait resolves when userdata is pushed, not on each access.
	// The object intentionally stays alive: the probe must never dereference
	// freed memory. It prevents treating this trait as a stale-pointer guard.
	LuaAPI::ObjectRegistry<TraitProbeObject> probeRegistry;
	TraitProbeObject probe { 27 };
	lua.new_usertype<TraitProbeObject>("TraitProbeObject", sol::no_constructor,
		"value", sol::property([](const TraitProbeObject& value) { return value.value; }));
	lua["traitProbe"] = TraitProbeHandle { probeRegistry.watch(&probe) };
	lua.script("assert(traitProbe.value == 27)");
	probeRegistry.invalidate(&probe);
	lua.script("assert(traitProbe.value == 27)");
}
