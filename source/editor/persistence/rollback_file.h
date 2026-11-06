//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_ROLLBACK_FILE_H_
#define RME_ROLLBACK_FILE_H_

#include <filesystem>
#include <system_error>

namespace rme::persistence {

enum class RollbackFileResult { Done, RemoveFailed, RestoreFailed };

// An empty backup means the target did not exist before the save.
[[nodiscard]] inline RollbackFileResult rollbackFile(const std::filesystem::path& backup,
	const std::filesystem::path& target, std::error_code& error) {
	error.clear();
	if (target.empty()) {
		if (backup.empty()) return RollbackFileResult::Done;
		error = std::make_error_code(std::errc::invalid_argument);
		return RollbackFileResult::RestoreFailed;
	}
	if (backup.empty()) {
		std::filesystem::remove(target, error);
		return error ? RollbackFileResult::RemoveFailed : RollbackFileResult::Done;
	}
	std::filesystem::copy_file(backup, target, std::filesystem::copy_options::overwrite_existing, error);
	return error ? RollbackFileResult::RestoreFailed : RollbackFileResult::Done;
}

} // namespace rme::persistence

#endif
