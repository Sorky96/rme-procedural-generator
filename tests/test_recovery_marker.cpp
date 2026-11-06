#include "editor/persistence/recovery_marker.h"

#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {
void check(bool condition, const char* message) {
	if (!condition) throw std::runtime_error(message);
}
}

int main() {
	using namespace rme::persistence;
	try {
		std::vector<std::string> firstSave(7);
		firstSave.insert(firstSave.end(), {"C:/fixture/new.otbm", "C:/fixture/new-house.xml", "C:/fixture/new-spawn.xml"});
		const auto encoded = serializeRecoveryMarker(firstSave);
		check(encoded.has_value(), "First-save marker serialization failed");
		const auto parsed = parseRecoveryMarker(*encoded);
		check(parsed && !parsed->legacy && parsed->files == firstSave, "First-save paths changed");
		for (size_t length = 0; length < encoded->size(); ++length) {
			check(!parseRecoveryMarker(std::string_view(*encoded).substr(0, length)), "Truncated versioned marker accepted");
		}
		check(!parseRecoveryMarker(*encoded + "extra\n"), "Trailing record accepted");
		auto changed = *encoded;
		changed.replace(changed.find("\n10\n"), 4, "\n11\n");
		check(!parseRecoveryMarker(changed), "Wrong record count accepted");
		changed = *encoded;
		changed.replace(changed.find("V1"), 2, "V2");
		check(!parseRecoveryMarker(changed), "Unknown version accepted as legacy");
		std::vector<std::string> backups {"C:/fixture/map.otbm~", "C:/fixture/za\xC5\xBC-house.xml~", "C:/fixture/map-spawn.xml~", "", "", "", ""};
		const auto saved = serializeRecoveryMarker(backups);
		const auto savedParsed = saved ? parseRecoveryMarker(*saved) : std::nullopt;
		check(savedParsed && savedParsed->files == backups, "Backup/UTF-8 round-trip failed");
		std::string legacy;
		for (const auto& file : backups) legacy += file + "\r\n";
		const auto old = parseRecoveryMarker(legacy);
		check(old && old->legacy && old->files == backups, "Complete legacy marker not recognized");
		check(!parseRecoveryMarker("C:/fixture/map.otbm~\n"), "Incomplete legacy backup slots accepted");
		check(!parseRecoveryMarker(std::string(7, '\n')), "Legacy marker without paths accepted");
		backups[1] = "C:/fixture/not-a-backup.xml";
		check(!serializeRecoveryMarker(backups), "Invalid backup suffix accepted");
		backups[1] = "C:/fixture/null";
		backups[1].push_back('\0');
		backups[1] += ".xml~";
		check(!serializeRecoveryMarker(backups), "NUL path accepted");
		firstSave.back() = "C:/fixture/bad\npath.xml";
		check(!serializeRecoveryMarker(firstSave), "Record injection accepted");
		std::istringstream stream(*encoded);
		const auto fromStream = readRecoveryMarker(stream);
		check(fromStream && fromStream->files == parsed->files, "Stream reader changed paths");
		std::istringstream oversized(std::string(maxRecoveryMarkerBytes + 1, 'x'));
		check(!readRecoveryMarker(oversized), "Oversized stream accepted");
		std::cout << "PASS: first save, backups, every truncated prefix, count/footer/version, legacy, paths and stream limit\n";
		return 0;
	} catch (const std::exception& error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
