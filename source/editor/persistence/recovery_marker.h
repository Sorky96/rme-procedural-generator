#pragma once

#include <charconv>
#include <istream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rme::persistence {

inline constexpr std::string_view recoveryMarkerHeader = "RME_SAVE_RECOVERY_V1";
inline constexpr std::string_view recoveryMarkerFooter = "RME_SAVE_RECOVERY_END";
inline constexpr size_t maxRecoveryMarkerBytes = 1024 * 1024;
inline constexpr size_t maxRecoveryMarkerFiles = 1024;
inline constexpr size_t recoveryBackupCount = 7;

struct RecoveryMarker {
	std::vector<std::string> files;
	bool legacy = false;
};

inline bool validRecoveryPaths(std::span<const std::string> files) {
	if (files.size() < recoveryBackupCount || files.size() > maxRecoveryMarkerFiles) return false;
	bool hasPath = false;
	for (size_t index = 0; index < files.size(); ++index) {
		const auto& path = files[index];
		if (path.find_first_of("\r\n") != std::string::npos || path.find('\0') != std::string::npos) return false;
		if (index < recoveryBackupCount) {
			if (!path.empty() && path.back() != '~') return false;
		} else if (path.empty()) {
			return false;
		}
		hasPath |= !path.empty();
	}
	return hasPath;
}

inline std::optional<std::string> serializeRecoveryMarker(std::span<const std::string> files) {
	if (!validRecoveryPaths(files)) return std::nullopt;
	std::string result(recoveryMarkerHeader);
	result += '\n';
	result += std::to_string(files.size());
	result += '\n';
	for (const auto& file : files) {
		if (file.size() > maxRecoveryMarkerBytes || result.size() > maxRecoveryMarkerBytes - file.size()) {
			return std::nullopt;
		}
		result += file;
		result += '\n';
	}
	result += recoveryMarkerFooter;
	result += '\n';
	if (result.size() > maxRecoveryMarkerBytes) return std::nullopt;
	return result;
}

inline std::optional<RecoveryMarker> parseRecoveryMarker(std::string_view input) {
	if (input.empty() || input.size() > maxRecoveryMarkerBytes || input.back() != '\n') return std::nullopt;
	std::vector<std::string> lines;
	while (!input.empty()) {
		const auto end = input.find('\n');
		if (end == std::string_view::npos || lines.size() >= maxRecoveryMarkerFiles + 3) return std::nullopt;
		auto line = input.substr(0, end);
		if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
		lines.emplace_back(line);
		input.remove_prefix(end + 1);
	}
	RecoveryMarker marker;
	if (lines.front() == recoveryMarkerHeader) {
		if (lines.size() < recoveryBackupCount + 3 || lines.back() != recoveryMarkerFooter) return std::nullopt;
		size_t count = 0;
		const auto& text = lines[1];
		const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), count);
		if (error != std::errc {} || end != text.data() + text.size() || count != lines.size() - 3) return std::nullopt;
		marker.files.assign(lines.begin() + 2, lines.end() - 1);
	} else {
		if (lines.front().starts_with("RME_SAVE_RECOVERY_")) return std::nullopt;
		// Legacy writers had no count/footer. Their known paths can be inspected
		// and recovered, but EOF cannot prove the optional target list complete.
		marker.legacy = true;
		marker.files = std::move(lines);
	}
	if (!validRecoveryPaths(marker.files)) return std::nullopt;
	return marker;
}

inline std::optional<RecoveryMarker> readRecoveryMarker(std::istream& input) {
	std::string bytes(maxRecoveryMarkerBytes + 1, '\0');
	input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
	const auto count = static_cast<size_t>(input.gcount());
	if (input.bad() || count > maxRecoveryMarkerBytes) return std::nullopt;
	bytes.resize(count);
	return parseRecoveryMarker(bytes);
}

} // namespace rme::persistence
