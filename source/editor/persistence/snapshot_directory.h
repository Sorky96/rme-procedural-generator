#ifndef RME_EDITOR_PERSISTENCE_SNAPSHOT_DIRECTORY_H_
#define RME_EDITOR_PERSISTENCE_SNAPSHOT_DIRECTORY_H_

#include <filesystem>
#include <string>
#include <system_error>

// Atomically claim a directory for one snapshot and all of its companion files.
inline std::filesystem::path reserveSnapshotDirectory(
	const std::filesystem::path& parent, const std::wstring& base, std::error_code& error
) {
	error.clear();
	for (unsigned int attempt = 0; attempt < 10000; ++attempt) {
		const auto name = attempt == 0 ? base : base + L"_" + std::to_wstring(attempt);
		const auto candidate = parent / name;
		if (std::filesystem::create_directory(candidate, error)) {
			return candidate;
		}
		if (error && error != std::errc::file_exists) {
			return {};
		}
		error.clear();
	}
	error = std::make_error_code(std::errc::file_exists);
	return {};
}

#endif
