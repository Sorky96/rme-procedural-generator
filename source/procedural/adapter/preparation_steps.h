// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <coroutine>
#include <exception>
#include <optional>
#include <utility>

namespace rme::procedural {

// Main-thread continuation. Destroying it releases the suspended stage and all
// its owned data. No callbacks or background work survive destruction.
template<typename Result>
class PreparationSteps {
public:
	struct promise_type {
		std::optional<Result> result;
		std::exception_ptr exception;

		PreparationSteps get_return_object() noexcept {
			return PreparationSteps(Handle::from_promise(*this));
		}
		std::suspend_always initial_suspend() noexcept { return {}; }
		std::suspend_always final_suspend() noexcept { return {}; }
		void return_value(Result value) { result.emplace(std::move(value)); }
		void unhandled_exception() noexcept { exception = std::current_exception(); }
	};

	PreparationSteps(PreparationSteps&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
	~PreparationSteps() { if (handle_) handle_.destroy(); }

	// Empty means the stage suspended and can be resumed again.
	std::optional<Result> advance() {
		handle_.resume();
		if (handle_.promise().exception) std::rethrow_exception(handle_.promise().exception);
		return std::move(handle_.promise().result);
	}

	Result finish() {
		for (;;) {
			if (auto result = advance()) return std::move(*result);
		}
	}

private:
	using Handle = std::coroutine_handle<promise_type>;
	explicit PreparationSteps(Handle handle) noexcept : handle_(handle) {}
	Handle handle_;
};

} // namespace rme::procedural
