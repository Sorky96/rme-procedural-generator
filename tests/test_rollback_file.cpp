#ifdef NDEBUG
#undef NDEBUG
#endif
#include "editor/persistence/rollback_file.h"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace fs = std::filesystem;
using rme::persistence::RollbackFileResult;
using rme::persistence::rollbackFile;

static void writeFile(const fs::path& path, const char* value) {
	std::ofstream output(path, std::ios::binary);
	output << value;
	output.close();
	assert(output.good());
}

static std::string readFile(const fs::path& path) {
	std::ifstream input(path, std::ios::binary);
	assert(input.is_open());
	return std::string(std::istreambuf_iterator<char>(input), {});
}

int main() {
	const auto id = std::chrono::steady_clock::now().time_since_epoch().count();
	const auto dir = fs::temp_directory_path() / ("rme-rollback-test-" + std::to_string(id));
	assert(fs::create_directory(dir));
	const auto target = dir / L"map-\u017c\u00f3\u0142\u0107.otbm";
	const auto backup = dir / L"map-\u017c\u00f3\u0142\u0107.otbm~";
	std::error_code error;
	assert(rollbackFile(backup, {}, error) == RollbackFileResult::RestoreFailed && error);
	assert(rollbackFile({}, target, error) == RollbackFileResult::Done && !error);
	writeFile(target, "partial");
	assert(rollbackFile({}, target, error) == RollbackFileResult::Done && !fs::exists(target));
	writeFile(target, "partial");
	writeFile(backup, "original");
	assert(rollbackFile(backup, target, error) == RollbackFileResult::Done);
	assert(readFile(target) == "original" && readFile(backup) == "original");

#ifdef _WIN32
	HANDLE locked = CreateFileW(target.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	assert(locked != INVALID_HANDLE_VALUE);
	assert(rollbackFile({}, target, error) == RollbackFileResult::RemoveFailed && error);
	assert(readFile(target) == "original");
	assert(rollbackFile(backup, target, error) == RollbackFileResult::RestoreFailed && error);
	assert(CloseHandle(locked));
	assert(rollbackFile(backup, target, error) == RollbackFileResult::Done && !error);
	assert(rollbackFile({}, target, error) == RollbackFileResult::Done && !error);
#endif
	assert(fs::remove(backup));
	assert(fs::remove(dir));
}
