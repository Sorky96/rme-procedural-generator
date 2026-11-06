//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "editor/persistence/map_backup_service.h"
#include "editor/persistence/snapshot_directory.h"
#include "editor/persistence/recovery_marker.h"
#include "editor/editor.h"
#include "map/map.h"
#include "io/iomap_otbm.h"
#include "util/file_system.h"
#include <ctime>
#include <fstream>
#include <filesystem>
#include <spdlog/spdlog.h>

std::string MapBackupService::GenerateTimestampString(bool compact) {
	std::time_t now = std::time(nullptr);
	std::tm tm_now = *std::localtime(&now);
	char time_buf[64];
	if (compact) {
		std::strftime(time_buf, sizeof(time_buf), "%Y%m%d_%H%M%S", &tm_now);
	} else {
		std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d-%H-%M-%S", &tm_now);
	}
	return std::string(time_buf);
}

bool MapBackupService::WriteCrashMarker(const std::vector<std::string>& files) {
	const auto contents = rme::persistence::serializeRecoveryMarker(files);
	if (!contents) return false;
	const std::filesystem::path marker_path((FileSystem::GetLocalDataDirectory() + ".saving.txt").ToStdWstring());
	std::ofstream f(marker_path, std::ios::out | std::ios::binary | std::ios::noreplace);
	if (!f) {
		return false;
	}
	f.write(contents->data(), static_cast<std::streamsize>(contents->size()));
	f.close();
	return !f.fail();
}

bool MapBackupService::ClearCrashMarker() {
	const std::filesystem::path marker_path((FileSystem::GetLocalDataDirectory() + ".saving.txt").ToStdWstring());
	std::error_code ec;
	return std::filesystem::remove(marker_path, ec) && !ec;
}

bool MapBackupService::CreateSnapshot(
	Editor& editor,
	std::string& out_backup_path,
	std::string& out_error,
	const std::string& operation_tag
) {
	const std::string time_str = GenerateTimestampString(true);

	FileName target_file;
	if (editor.map.hasFile()) {
		FileName src(wxstr(editor.map.getFilename()));
		target_file = src;
		target_file.SetName(src.GetName() + "_" + operation_tag + "_" + time_str);
	} else {
		std::string dir = nstr(FileSystem::GetLocalDataDirectory());
		target_file.Assign(wxstr(dir + "untitled_" + operation_tag + "_" + time_str + ".otbm"));
	}

	// Reserve a private directory before writing any member of the snapshot.
	// A timestamp alone is not unique, and checking file existence before
	// opening the files would still allow another process to win the race.
	const std::filesystem::path parent(target_file.GetPath(wxPATH_GET_VOLUME).ToStdWstring());
	const std::wstring directory_base = target_file.GetName().ToStdWstring() + L"_snapshot";
	std::error_code ec;
	const auto snapshot_directory = reserveSnapshotDirectory(parent, directory_base, ec);
	if (snapshot_directory.empty()) {
		out_error = "Could not reserve a snapshot directory in: " + nstr(wxString(parent.wstring())) + ": " + ec.message();
		return false;
	}
	target_file.SetPath(wxString(snapshot_directory.wstring()));
	out_backup_path = nstr(target_file.GetFullPath());

	const std::string original_spawnfile = editor.map.getSpawnFilename();
	const std::string original_monsterfile = editor.map.getMonsterFilename();
	const std::string original_npcfile = editor.map.getNpcFilename();
	const std::string original_housefile = editor.map.getHouseFilename();
	const std::string original_zonefile = editor.map.getZoneFilename();
	const std::string original_waypointfile = editor.map.getWaypointFilename();
	const bool original_changed = editor.map.hasChanged();
	const bool original_unnamed = editor.map.isUnnamed();

	const std::string snapshot_base = nstr(target_file.GetName());
	editor.map.setSpawnFilename(snapshot_base + "-spawn.xml");
	editor.map.setMonsterFilename(snapshot_base + "-monster.xml");
	editor.map.setNpcFilename(snapshot_base + "-npc.xml");
	editor.map.setHouseFilename(snapshot_base + "-house.xml");
	editor.map.setZoneFilename(snapshot_base + "-zones.xml");
	editor.map.setWaypointFilename(snapshot_base + "-waypoint.xml");

	IOMapOTBM mapsaver(editor.map.getVersion());
	const bool save_ok = mapsaver.saveMap(editor.map, target_file);

	// Restore original filenames, unnamed state, and preserve dirty state on both success and failure paths
	editor.map.setSpawnFilename(original_spawnfile);
	editor.map.setMonsterFilename(original_monsterfile);
	editor.map.setNpcFilename(original_npcfile);
	editor.map.setHouseFilename(original_housefile);
	editor.map.setZoneFilename(original_zonefile);
	editor.map.setWaypointFilename(original_waypointfile);
	editor.map.setUnnamed(original_unnamed);
	if (!original_changed && editor.map.hasChanged()) {
		editor.map.clearChanges();
	}

	if (!save_ok) {
		// Keep partial files for inspection; deleting by name could remove files
		// created by another process after the directory was reserved.
		out_error = "Failed to save backup map snapshot. Partial files may remain in: " + nstr(wxString(snapshot_directory.wstring()));
		spdlog::error("MapBackupService::CreateSnapshot: {}", out_error);
		return false;
	}

	spdlog::info("MapBackupService::CreateSnapshot: Created map snapshot at {}", out_backup_path);
	return true;
}
