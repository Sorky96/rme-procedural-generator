//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include "ui/dialog_util.h"
#include "util/file_system.h"
#include "editor/persistence/editor_persistence.h"
#include "editor/persistence/map_backup_service.h"
#include "editor/persistence/rollback_file.h"
#include "editor/editor.h"
#include "editor/action.h"
#include "editor/action_queue.h"
#include "editor/selection.h"
#include "map/map.h"
#include "io/iomap.h"
#include "io/map_xml_io.h"
#include "app/settings.h"
#include "app/managers/version_manager.h"
#include "ui/gui.h"
#include "lua/lua_script_manager.h"

#include <fstream>
#include <filesystem>
#include <format>
#include <algorithm>
#include <array>
#include <unordered_map>
#include <spdlog/spdlog.h>

void EditorPersistence::loadMap(Editor& editor, const FileName& fn, const MapLoadOptions& load_options) {
	MapVersion ver;
	if (!IOMapOTBM::getVersionInfo(fn, ver)) {
		throw std::runtime_error("Could not open file \"" + nstr(fn.GetFullPath()) + "\".\nThis is not a valid OTBM file or it does not exist.");
	}

	const auto loaded_major = static_cast<uint32_t>(g_version.GetCurrentVersion().getOTBVersion().format_version);
	const bool mismatch = (g_version.GetCurrentVersion().getProtocolID() != ver.client) ||
		(ver.items_major != 0 && loaded_major != 0 && loaded_major != ver.items_major);
	if (mismatch && !load_options.force_client_mismatch) {
		throw std::runtime_error(std::format("Client version mismatch. Expected protocol {} (OTB major {}) but got protocol {} (OTB major {})", ver.client, ver.items_major, g_version.GetCurrentVersion().getProtocolID(), loaded_major));
	}

	// Report the file being opened and its size up front; the loader then
	// reports its own stages (decompression, tile blocks, node types).
	std::error_code size_ec;
	const auto file_bytes = std::filesystem::file_size(std::filesystem::path(fn.GetFullPath().utf8_string()), size_ec);
	wxString load_message = wxstr(std::format("Loading {}", nstr(fn.GetFullName())));
	if (!size_ec) {
		load_message = wxstr(std::format("Loading {} ({:.1f} MB)", nstr(fn.GetFullName()), static_cast<double>(file_bytes) / (1024.0 * 1024.0)));
	}
	ScopedLoadingBar loadingBar(load_message);
	editor.map.open(std::string(fn.GetFullPath().utf8_str()));
}

bool EditorPersistence::saveMap(Editor& editor, FileName filename, bool showdialog) {
	const std::filesystem::path marker_path((FileSystem::GetLocalDataDirectory() + ".saving.txt").ToStdWstring());
	std::error_code marker_error;
	const auto marker_status = std::filesystem::symlink_status(marker_path, marker_error);
	if ((marker_error && marker_error != std::errc::no_such_file_or_directory) || marker_status.type() != std::filesystem::file_type::not_found) {
		spdlog::error("Cannot save while a recovery marker exists or cannot be checked");
		if (showdialog) {
			DialogUtil::ShowTextBox(g_gui.root, "Save failed", "An earlier save still requires recovery. Resolve the recovery marker before saving another map.\n\n" + FileSystem::GetLocalDataDirectory() + ".saving.txt");
		}
		return false;
	}
	std::string savefile = filename.GetFullPath().mb_str(wxConvUTF8).data();
	bool save_as = false;

	if (savefile.empty()) {
		savefile = editor.map.getFilename();
	} else {
		FileName c1(wxstr(savefile));
		FileName c2(wxstr(editor.map.getFilename()));
		save_as = c1 != c2;
	}

	const std::string prev_filename = editor.map.getFilename();
	const std::string prev_name = editor.map.getName();
	const bool prev_unnamed = editor.map.unnamed;
	const std::string prev_spawn = editor.map.getSpawnFilename();
	const std::string prev_monster = editor.map.getMonsterFilename();
	const std::string prev_npc = editor.map.getNpcFilename();
	const std::string prev_house = editor.map.getHouseFilename();
	const std::string prev_zone = editor.map.getZoneFilename();
	const std::string prev_waypoint = editor.map.getWaypointFilename();

	// A copy must own its XML files. Reusing the source names would overwrite
	// the original map's companions when both maps are in the same directory.
	if (save_as || editor.map.unnamed) {
		const FileName destination(wxstr(savefile));
		const std::filesystem::path directory(destination.GetPath(wxPATH_GET_VOLUME).ToStdWstring());
		const auto stem = destination.GetName();
		constexpr std::array suffixes { "-spawn.xml", "-monster.xml", "-npc.xml", "-house.xml", "-zones.xml" };
		const auto waypointFormat = static_cast<Config::WaypointStorageFormat>(g_settings.getInteger(Config::WAYPOINT_STORAGE_FORMAT));
		const bool saveWaypointXml = waypointFormat == Config::WaypointStorageFormat::XML || waypointFormat == Config::WaypointStorageFormat::BOTH;
		// The waypoint XML name is derived from the map name on load; unlike the
		// other companions it cannot be changed in the OTBM header.
		{
			const wxString waypointName = stem + "-waypoint.xml";
			std::error_code ec;
			const auto status = std::filesystem::symlink_status(directory / std::filesystem::path(waypointName.ToStdWstring()), ec);
			if ((ec && ec != std::errc::no_such_file_or_directory) || status.type() != std::filesystem::file_type::not_found) {
				spdlog::error("Cannot use destination waypoint file '{}': {}", nstr(waypointName), ec ? ec.message() : "file already exists");
				if (showdialog) {
					DialogUtil::PopupDialog("Save failed", "The destination waypoint XML already exists or cannot be checked. Choose another map name.", wxOK | wxICON_ERROR);
				}
				return false;
			}
		}
		wxString prefix;
		bool found = false;
		for (unsigned int index = 0; index < 10000 && !found; ++index) {
			prefix = stem;
			if (index != 0) {
				prefix += wxstr(std::format("-{}", index));
			}
			found = true;
			for (const auto* suffix : suffixes) {
				const auto candidate = directory / std::filesystem::path((prefix + wxString::FromUTF8(suffix)).ToStdWstring());
				std::error_code ec;
				const auto status = std::filesystem::symlink_status(candidate, ec);
				if (ec && ec != std::errc::no_such_file_or_directory) {
					spdlog::error("Cannot inspect companion file {}: {}", nstr(prefix + wxString::FromUTF8(suffix)), ec.message());
					if (showdialog) {
						DialogUtil::PopupDialog("Save failed", "Could not inspect the destination's companion files.", wxOK | wxICON_ERROR);
					}
					return false;
				}
				if (status.type() != std::filesystem::file_type::not_found) {
					found = false;
					break;
				}
			}
		}
		if (!found) {
			if (showdialog) {
				DialogUtil::PopupDialog("Save failed", "Could not find unused names for the destination's companion files.", wxOK | wxICON_ERROR);
			}
			return false;
		}
		auto companion = [&prefix](const char* suffix) { return std::string((prefix + wxString::FromUTF8(suffix)).utf8_str()); };
		editor.map.setHouseFilename(companion("-house.xml"));
		if (editor.map.getVersion().otbm == MAP_OTBM_5) {
			editor.map.setMonsterFilename(companion("-monster.xml"));
			editor.map.setNpcFilename(companion("-npc.xml"));
			if (!editor.map.zones.empty()) {
				editor.map.setZoneFilename(companion("-zones.xml"));
			} else {
				editor.map.setZoneFilename("");
			}
		} else {
			editor.map.setSpawnFilename(companion("-spawn.xml"));
		}
		editor.map.setWaypointFilename(saveWaypointXml ? std::string((stem + "-waypoint.xml").utf8_str()) : "");
		editor.map.unnamed = false;
	}

	// File object to convert between local paths etc.
	FileName converter;
	converter.Assign(wxstr(savefile));
	std::string map_path = std::string(converter.GetPath(wxPATH_GET_SEPARATOR | wxPATH_GET_VOLUME).utf8_str());
		// Backup, recovery and the XML writer must address the same resolved
		// file, including companions in subdirectories and constrained paths.
		auto companionPath = [&converter](const std::string& configured) {
			return configured.empty() ? std::string{} : MapXMLIO::normalizeMapFilePaths(converter, configured).first;
		};
		const auto house_path = companionPath(editor.map.getHouseFilename());
		const auto spawn_path = companionPath(editor.map.getSpawnFilename());
		const auto monster_path = companionPath(editor.map.getMonsterFilename());
		const auto npc_path = companionPath(editor.map.getNpcFilename());
		const auto zone_path = companionPath(editor.map.getZoneFilename());
		const auto waypoint_path = companionPath(editor.map.getWaypointFilename());

	// Make temporary backups
	std::string backup_otbm;
	std::string backup_house;
	std::string backup_spawn;
	std::string backup_monster;
	std::string backup_npc;
	std::string backup_zone;
	std::string backup_waypoint;

	std::vector<std::pair<std::string, std::string>> created_backups;
	bool backup_failed = false;
	std::string failed_backup_path;
	auto createBackup = [&](const std::string& sourcePath, std::string& backupPath) {
		if (backup_failed) {
			backupPath.clear();
			return;
		}
		const std::filesystem::path source(wxstr(sourcePath).ToStdWstring());
		const std::filesystem::path backup(wxstr(backupPath).ToStdWstring());
		std::error_code ec;
		const auto status = std::filesystem::symlink_status(backup, ec);
		if ((ec && ec != std::errc::no_such_file_or_directory) || status.type() != std::filesystem::file_type::not_found) {
			spdlog::error("EditorPersistence: Backup destination '{}' already exists or cannot be checked: {}", backupPath, ec.message());
			failed_backup_path = backupPath;
			backupPath.clear();
			backup_failed = true;
			return;
		}
		ec.clear();
		std::filesystem::copy_file(source, backup, std::filesystem::copy_options::none, ec);
		if (ec) {
			spdlog::error("EditorPersistence: Failed to create backup '{}' -> '{}': {}", sourcePath, backupPath, ec.message());
			failed_backup_path = backupPath;
			backupPath.clear();
			backup_failed = true;
			return;
		}
		created_backups.emplace_back(sourcePath, backupPath);
	};

	if (converter.FileExists()) {
		backup_otbm = map_path + std::string(converter.GetName().utf8_str()) + ".otbm~";
		createBackup(savefile, backup_otbm);
	}

		auto backupCompanion = [&createBackup](const std::string& path, std::string& backup) {
			if (!path.empty() && FileName(wxstr(path)).FileExists()) {
				backup = path + "~";
				createBackup(path, backup);
			}
		};
		backupCompanion(house_path, backup_house);

	if (editor.map.getVersion().otbm == MAP_OTBM_5) {
			backupCompanion(monster_path, backup_monster);
			backupCompanion(npc_path, backup_npc);
			backupCompanion(zone_path, backup_zone);
	} else {
			backupCompanion(spawn_path, backup_spawn);
	}
		backupCompanion(waypoint_path, backup_waypoint);

	if (backup_failed) {
		std::vector<std::string> failed_cleanup;
		for (auto it = created_backups.rbegin(); it != created_backups.rend(); ++it) {
			std::error_code ec;
			std::filesystem::remove(std::filesystem::path(wxstr(it->second).ToStdWstring()), ec);
			if (ec) {
				spdlog::error("EditorPersistence: Failed to remove unused backup '{}': {}", it->second, ec.message());
				failed_cleanup.push_back(it->second);
			}
		}
		if (showdialog) {
			wxString message = "Could not create the temporary backup. No map was saved.\n\nBackup path: " + wxstr(failed_backup_path);
			if (!failed_cleanup.empty()) {
				message += "\n\nUnused backup copies remain at:\n";
				for (const auto& path : failed_cleanup) {
					message += wxstr(path) + "\n";
				}
			}
			DialogUtil::PopupDialog("Save failed", message, wxOK | wxICON_ERROR);
		}
		editor.map.setSpawnFilename(prev_spawn);
		editor.map.setMonsterFilename(prev_monster);
		editor.map.setNpcFilename(prev_npc);
		editor.map.setHouseFilename(prev_house);
		editor.map.setZoneFilename(prev_zone);
		editor.map.setWaypointFilename(prev_waypoint);
		editor.map.unnamed = prev_unnamed;
		return false;
	}

	// Originals stay in place until backup copies and the recovery marker exist.
	std::vector<std::string> recovery_files {backup_otbm, backup_house, backup_spawn, backup_monster, backup_npc, backup_zone, backup_waypoint};
	auto recordNewTarget = [&recovery_files](const std::string& backup, const std::string& target) {
		if (backup.empty() && !target.empty()) {
			recovery_files.push_back(target);
		}
	};
	recordNewTarget(backup_otbm, savefile);
		if (!house_path.empty()) recordNewTarget(backup_house, house_path);
	if (editor.map.getVersion().otbm == MAP_OTBM_5) {
			if (!monster_path.empty()) recordNewTarget(backup_monster, monster_path);
			if (!npc_path.empty()) recordNewTarget(backup_npc, npc_path);
			if (!zone_path.empty()) recordNewTarget(backup_zone, zone_path);
		} else if (!spawn_path.empty()) {
			recordNewTarget(backup_spawn, spawn_path);
	}
		if (!waypoint_path.empty()) recordNewTarget(backup_waypoint, waypoint_path);
	if (!MapBackupService::WriteCrashMarker(recovery_files)) {
		spdlog::error("EditorPersistence: Failed to create recovery marker; map was not saved");
		if (showdialog) {
			DialogUtil::PopupDialog("Save failed", "Could not create the recovery marker. No map was saved; any backup copies have been kept.", wxOK | wxICON_ERROR);
		}
		editor.map.setSpawnFilename(prev_spawn);
		editor.map.setMonsterFilename(prev_monster);
		editor.map.setNpcFilename(prev_npc);
		editor.map.setHouseFilename(prev_house);
		editor.map.setZoneFilename(prev_zone);
		editor.map.setWaypointFilename(prev_waypoint);
		editor.map.unnamed = prev_unnamed;
		return false;
	}

	bool marker_cleared = false;
	{

		// Set up the Map paths
		wxFileName fn = wxstr(savefile);
		editor.map.filename = fn.GetFullPath().mb_str(wxConvUTF8);
		editor.map.name = fn.GetFullName().mb_str(wxConvUTF8);

		if (showdialog) {
			g_gui.CreateLoadBar("Saving map...", true, "Saving");
		}

		// Perform the actual save
		IOMapOTBM mapsaver(editor.map.getVersion());
		bool success = mapsaver.saveMap(editor.map, fn);
		const bool cancelled = showdialog && g_gui.WasLoadCancelled();
		bool keep_recovery_info = false;

		// Check for errors...
		if (!success) {
			if (cancelled) {
				// Keep the bar up while the partial write is rolled back so the
				// user sees the cancellation being processed instead of a frozen
				// window; it is destroyed below, after the rollback.
				g_gui.SetLoadText("Cancelling...");
				g_gui.SetStatusText("Saving cancelled.");
			} else if (showdialog) {
				// Error path: the bar has to go before the modal error dialog.
				g_gui.DestroyLoadBar();
			}

			// Restore the temporary backups. A failed restore leaves the target
			// partial and the previous data in the "~" backup, so it must be
			// reported and kept recoverable instead of silently ignored.
			std::vector<std::string> failed_restores;
			std::vector<std::string> failed_removals;

			auto restoreBackup = [&failed_restores, &failed_removals](const std::string& backupPath, const std::string& targetPath) {
				const std::filesystem::path target(wxstr(targetPath).ToStdWstring());
				std::error_code ec;
				const auto result = rme::persistence::rollbackFile(std::filesystem::path(wxstr(backupPath).ToStdWstring()), target, ec);
				if (result == rme::persistence::RollbackFileResult::RemoveFailed) {
					spdlog::error("EditorPersistence: Failed to remove incomplete file '{}': {}", targetPath, ec.message());
					failed_removals.push_back(targetPath);
				} else if (result == rme::persistence::RollbackFileResult::RestoreFailed) {
					spdlog::error("EditorPersistence: Failed to restore backup '{}' -> '{}': {}", backupPath, targetPath, ec.message());
					failed_restores.push_back(backupPath);
				}
			};

			restoreBackup(backup_otbm, savefile);
			if (!editor.map.getHouseFilename().empty()) {
					restoreBackup(backup_house, house_path);
			}
			if (editor.map.getVersion().otbm == MAP_OTBM_5) {
				if (!editor.map.getMonsterFilename().empty()) {
						restoreBackup(backup_monster, monster_path);
				}
				if (!editor.map.getNpcFilename().empty()) {
						restoreBackup(backup_npc, npc_path);
				}
				if (!editor.map.getZoneFilename().empty()) {
						restoreBackup(backup_zone, zone_path);
				}
			} else {
				if (!editor.map.getSpawnFilename().empty()) {
						restoreBackup(backup_spawn, spawn_path);
				}
			}
			if (!editor.map.getWaypointFilename().empty()) {
					restoreBackup(backup_waypoint, waypoint_path);
			}

			if (!failed_restores.empty() || !failed_removals.empty()) {
				// Keep backups and the marker when any rollback operation fails.
				keep_recovery_info = true;

				wxString message = "The save could not be rolled back completely.";
				if (!failed_restores.empty()) {
					message += "\n\nBackups that could not be restored:\n";
					for (const std::string& path : failed_restores) message += wxstr(path) + "\n";
					message += "\nRecover these backups before saving again.";
				}
				if (!failed_removals.empty()) {
					message += "\n\nNew incomplete files that could not be removed:\n";
					for (const std::string& path : failed_removals) message += wxstr(path) + "\n";
					message += "\nInspect and remove these files manually. Their paths are recorded in the recovery marker.";
				}
				message += "\n\nThe recovery marker and backups have been kept. Resolve them before saving again.";
				DialogUtil::PopupDialog("Save failed", message, wxOK | wxICON_ERROR);
			} else if (!cancelled) {
				DialogUtil::PopupDialog("Error", "Could not save, unable to open target for writing.", wxOK);
			}
		}

		if (showdialog) {
			g_gui.DestroyLoadBar();
		}

		// Remove the temporary save runfile, unless the rollback could not
		// restore everything: then it has to survive so the next start can
		// offer to recover the previous files.
		if (!keep_recovery_info) {
			marker_cleared = MapBackupService::ClearCrashMarker();
			if (!marker_cleared) {
				spdlog::error("EditorPersistence: Could not remove recovery marker; backups have been kept");
				if (showdialog) {
					DialogUtil::PopupDialog("Recovery marker retained", "The recovery marker could not be removed. Backup copies have been kept; resolve the marker before another save.", wxOK | wxICON_WARNING);
				}
			}
		}

		// If failure, don't run the rest of the function
		if (!success) {
			if (marker_cleared) {
				for (const auto& [source, backup] : created_backups) {
					std::error_code ec;
					std::filesystem::remove(std::filesystem::path(wxstr(backup).ToStdWstring()), ec);
					if (ec) {
						spdlog::warn("EditorPersistence: Could not remove unused backup '{}': {}", backup, ec.message());
					}
				}
			}
			editor.map.filename = prev_filename;
			editor.map.name = prev_name;
			editor.map.setSpawnFilename(prev_spawn);
			editor.map.setMonsterFilename(prev_monster);
			editor.map.setNpcFilename(prev_npc);
			editor.map.setHouseFilename(prev_house);
			editor.map.setZoneFilename(prev_zone);
			editor.map.setWaypointFilename(prev_waypoint);
			editor.map.unnamed = prev_unnamed;
			return false;
		}
	}

	// Move to permanent backup
	if (!marker_cleared) {
		// Keep backups referenced by the retained recovery marker.
	} else if (!save_as && g_settings.getInteger(Config::ALWAYS_MAKE_BACKUP)) {
		const std::string date_str = MapBackupService::GenerateTimestampString(false);
		std::vector<std::string> failed_archives;
		for (const auto& [source, backup] : created_backups) {
			const std::filesystem::path source_path(wxstr(source).ToStdWstring());
			const std::filesystem::path backup_path(wxstr(backup).ToStdWstring());
			const auto archive = source_path.parent_path() / (source_path.stem().wstring() + L"." + wxstr(date_str).ToStdWstring() + source_path.extension().wstring());
			std::error_code ec;
			std::filesystem::copy_file(backup_path, archive, std::filesystem::copy_options::none, ec);
			if (!ec) {
				std::filesystem::remove(backup_path, ec);
			}
			if (ec) {
				spdlog::error("EditorPersistence: Could not archive backup '{}': {}", backup, ec.message());
				failed_archives.push_back(backup);
			}
		}
		if (showdialog && !failed_archives.empty()) {
			wxString message = "The map was saved, but some backups could not be archived. They remain at:\n\n";
			for (const auto& path : failed_archives) {
				message += wxstr(path) + "\n";
			}
			DialogUtil::PopupDialog("Backup warning", message, wxOK | wxICON_WARNING);
		}
	} else {
		// Delete the temporary files
		for (const auto& [source, backup] : created_backups) {
			std::error_code ec;
			std::filesystem::remove(std::filesystem::path(wxstr(backup).ToStdWstring()), ec);
			if (ec) {
				spdlog::warn("EditorPersistence: Could not remove unused backup '{}': {}", backup, ec.message());
			}
		}
	}

	editor.map.clearChanges();
	return true;
}

void EditorPersistence::importTowns(Editor& editor, Map& imported_map, const Position& offset, ImportType house_import_type, std::unordered_map<uint32_t, uint32_t>& town_id_map) {
	if (house_import_type == IMPORT_DONT) {
		return;
	}

	for (auto tit = imported_map.towns.begin(); tit != imported_map.towns.end();) {
		Town* imported_town = tit->second.get();
		Town* current_town = editor.map.towns.getTown(imported_town->getID());

		Position oldexit = imported_town->getTemplePosition();
		Position newexit = oldexit + offset;

		bool skip = false;
		switch (house_import_type) {
			case IMPORT_MERGE: {
				town_id_map[imported_town->getID()] = imported_town->getID();
				if (current_town) {
					skip = true;
				}
				break;
			}
			case IMPORT_SMART_MERGE: {
				if (current_town) {
					// Compare and insert/merge depending on parameters
					if (current_town->getName() == imported_town->getName() && current_town->getID() == imported_town->getID()) {
						// Just add to map
						town_id_map[imported_town->getID()] = current_town->getID();
						skip = true;
					} else {
						// Conflict! Find a new id and replace old
						uint32_t old_id = imported_town->getID();
						uint32_t new_id = editor.map.towns.getEmptyID();
						imported_town->setID(new_id);
						town_id_map[old_id] = new_id;
					}
				} else {
					town_id_map[imported_town->getID()] = imported_town->getID();
				}
				break;
			}
			case IMPORT_INSERT: {
				// Find a new id and replace old
				uint32_t old_id = imported_town->getID();
				uint32_t new_id = editor.map.towns.getEmptyID();
				imported_town->setID(new_id);
				town_id_map[old_id] = new_id;
				break;
			}
			case IMPORT_DONT: {
				skip = true;
				break;
			}
		}

		if (skip) {
			++tit;
			continue;
		}

		if (newexit.isValid()) {
			imported_town->setTemplePosition(newexit);
			editor.map.getOrCreateTile(newexit)->getLocation()->increaseTownCount();
		}

		uint32_t town_id = imported_town->getID();
		if (!editor.map.towns.addTown(std::move(tit->second))) {
			spdlog::warn("Failed to add town {} during import (duplicate ID)", town_id);
		}

		tit = imported_map.towns.erase(tit);
	}
}

void EditorPersistence::importHouses(Editor& editor, Map& imported_map, const Position& offset, ImportType house_import_type, const std::unordered_map<uint32_t, uint32_t>& town_id_map, std::unordered_map<uint32_t, uint32_t>& house_id_map) {
	if (house_import_type == IMPORT_DONT) {
		return;
	}

	for (auto hit = imported_map.houses.begin(); hit != imported_map.houses.end();) {
		House* imported_house = hit->second.get();
		House* current_house = editor.map.houses.getHouse(imported_house->getID());

		if (auto it = town_id_map.find(imported_house->townid); it != town_id_map.end()) {
			imported_house->townid = it->second;
		} else {
			imported_house->townid = 0;
		}

		Position oldexit = imported_house->getExit();
		imported_house->setExit(nullptr, Position()); // Reset it

		bool skip = false;
		switch (house_import_type) {
			case IMPORT_MERGE: {
				house_id_map[imported_house->getID()] = imported_house->getID();
				if (current_house) {
					skip = true;
					Position newexit = oldexit + offset;
					if (newexit.isValid()) {
						current_house->setExit(&editor.map, newexit);
					}
				}
				break;
			}
			case IMPORT_SMART_MERGE: {
				if (current_house) {
					// Compare and insert/merge depending on parameters
					if (current_house->name == imported_house->name && current_house->townid == imported_house->townid) {
						// Just add to map
						house_id_map[imported_house->getID()] = current_house->getID();
						skip = true;
						Position newexit = oldexit + offset;
						if (newexit.isValid()) {
							current_house->setExit(&editor.map, newexit);
						}
					} else {
						// Conflict! Find a newd id and replace old
						uint32_t new_id = editor.map.houses.getEmptyID();
						house_id_map[imported_house->getID()] = new_id;
						imported_house->setID(new_id);
					}
				} else {
				}
				break;
			}
			case IMPORT_INSERT: {
				// Find a newd id and replace old
				uint32_t new_id = editor.map.houses.getEmptyID();
				house_id_map[imported_house->getID()] = new_id;
				imported_house->setID(new_id);
				break;
			}
			default:
				break;
		}

		if (skip) {
			++hit;
			continue;
		}

		Position newexit = oldexit + offset;
		if (newexit.isValid()) {
			imported_house->setExit(&editor.map, newexit);
		}
		uint32_t house_id = imported_house->getID();
		if (!editor.map.houses.addHouse(std::move(hit->second))) {
			spdlog::warn("Failed to add house {} during import (duplicate ID)", house_id);
		}

		hit = imported_map.houses.erase(hit);
	}
}

void EditorPersistence::importSpawns(Editor& editor, Map& imported_map, const Position& offset, ImportType spawn_import_type, std::map<Position, std::unique_ptr<Spawn>>& spawn_map) {
	if (spawn_import_type == IMPORT_DONT) {
		return;
	}

	for (auto siter = imported_map.spawns.begin(); siter != imported_map.spawns.end();) {
		Position old_spawn_pos = *siter;
		Position new_spawn_pos = *siter + offset;
		bool skip = false;

		switch (spawn_import_type) {
			case IMPORT_SMART_MERGE:
			case IMPORT_INSERT:
			case IMPORT_MERGE: {
				Tile* imported_tile = imported_map.getTile(old_spawn_pos);
				if (imported_tile) {
					ASSERT(imported_tile->spawn);
					spawn_map[new_spawn_pos] = std::move(imported_tile->spawn);
				}
				break;
			}
			case IMPORT_DONT: {
				skip = true;
				break;
			}
		}

		if (skip) {
			++siter;
		} else {
			Tile* imported_tile = imported_map.getTile(old_spawn_pos);
			if (imported_tile) {
				siter = imported_map.spawns.erase(siter);
			} else {
				++siter;
			}
		}
	}
}

bool EditorPersistence::importMap(Editor& editor, FileName filename, int import_x_offset, int import_y_offset, ImportType house_import_type, ImportType spawn_import_type) {
	editor.selection.clear();
	editor.actionQueue->clear();

	Map imported_map;
	bool loaded = imported_map.open(std::string(filename.GetFullPath().utf8_str()));

	if (!loaded) {
		DialogUtil::PopupDialog("Error", "Error loading map!\n" + imported_map.getError(), wxOK | wxICON_INFORMATION);
		return false;
	}
	DialogUtil::ListDialog("Warning", imported_map.getWarnings());

	Position offset(import_x_offset, import_y_offset, 0);

	g_gui.CreateLoadBar("Merging maps...");

	std::unordered_map<uint32_t, uint32_t> town_id_map;
	town_id_map.reserve(imported_map.towns.count());
	std::unordered_map<uint32_t, uint32_t> house_id_map;
	house_id_map.reserve(imported_map.houses.count());

	importTowns(editor, imported_map, offset, house_import_type, town_id_map);
	importHouses(editor, imported_map, offset, house_import_type, town_id_map, house_id_map);

	std::unordered_map<uint16_t, uint16_t> zone_id_map;
	if (editor.map.getVersion().otbm == MAP_OTBM_5 && imported_map.getVersion().otbm == MAP_OTBM_5) {
		zone_id_map.reserve(imported_map.zones.size());
		for (const auto& imported_zone : imported_map.zones) {
			if (const auto* existing = editor.map.findZone(imported_zone.name)) {
				zone_id_map[imported_zone.id] = existing->id;
			} else {
				const uint16_t new_id = editor.map.addZone(imported_zone.name);
				if (new_id != 0) {
					zone_id_map[imported_zone.id] = new_id;
				}
			}
		}
	}

	std::map<Position, std::unique_ptr<Spawn>> spawn_map;
	importSpawns(editor, imported_map, offset, spawn_import_type, spawn_map);

	// Plain merge of waypoints, very simple! :)
	for (auto& [name, waypoint] : imported_map.waypoints) {
		waypoint->pos += offset;
		editor.map.waypoints.addWaypoint(std::move(waypoint));
	}
	imported_map.waypoints.waypoints.clear();

	uint64_t tiles_merged = 0;
	uint64_t tiles_to_import = imported_map.getTileCount();

	bool resizemap = false;
	bool resize_asked = false;
	int newsize_x = editor.map.getWidth();
	int newsize_y = editor.map.getHeight();
	int discarded_tiles = 0;

	for (MapIterator mit = imported_map.begin(); mit != imported_map.end(); ++mit) {
		if (tiles_merged % 8092 == 0) {
			g_gui.SetLoadDone(static_cast<int>(100.0 * static_cast<double>(tiles_merged) / static_cast<double>(tiles_to_import)));
		}
		++tiles_merged;

		Tile* import_tile = mit->get();
		Position new_pos = import_tile->getPosition() + offset;
		if (!new_pos.isValid()) {
			++discarded_tiles;
			continue;
		}

		if (!resizemap && (new_pos.x > editor.map.getWidth() || new_pos.y > editor.map.getHeight())) {
			if (resize_asked) {
				++discarded_tiles;
				continue;
			}
			
			resize_asked = true;
			int ret = DialogUtil::PopupDialog("Collision", "The imported tiles are outside the current map scope. Do you want to resize the map? (Else additional tiles will be removed)", wxYES | wxNO);
			if (ret == wxID_YES) {
				// ...
				resizemap = true;
			} else {
				++discarded_tiles;
				continue;
			}
		}

		newsize_x = std::max(newsize_x, int(new_pos.x));
		newsize_y = std::max(newsize_y, int(new_pos.y));

		std::unique_ptr<Tile> moved_tile = imported_map.setTile(import_tile->getPosition(), nullptr);
		TileLocation* location = editor.map.createTileL(new_pos);

		// Check if we should update any houses
		uint32_t new_houseid = 0;
		if (auto it = house_id_map.find(import_tile->getHouseID()); it != house_id_map.end()) {
			new_houseid = it->second;
		}

		House* house = editor.map.houses.getHouse(new_houseid);
		if (import_tile->isHouseTile() && house_import_type != IMPORT_DONT && house) {
			// We need to notify houses of the tile moving
			house->removeTile(import_tile);
			import_tile->setLocation(location);
			house->addTile(import_tile);
		} else {
			import_tile->setLocation(location);
		}

		if (offset != Position(0, 0, 0)) {
			for (auto& item : import_tile->items) {
				if (Teleport* teleport = dynamic_cast<Teleport*>(item.get())) {
					teleport->setDestination(teleport->getDestination() + offset);
				}
			}
		}

		Tile* old_tile = editor.map.getTile(new_pos);
		if (old_tile) {
			editor.map.removeSpawn(old_tile);
		}

		import_tile->spawn.reset();

		if (editor.map.getVersion().otbm == MAP_OTBM_5) {
			if (import_tile->hasZones()) {
				std::vector<uint16_t> remapped_zones;
				for (const uint16_t zid : import_tile->getZoneIds()) {
					if (auto it = zone_id_map.find(zid); it != zone_id_map.end()) {
						remapped_zones.push_back(it->second);
					}
				}
				std::sort(remapped_zones.begin(), remapped_zones.end());
				const auto last = std::unique(remapped_zones.begin(), remapped_zones.end());
				remapped_zones.erase(last, remapped_zones.end());
				import_tile->setZoneIds(remapped_zones);
			}
		} else if (import_tile->hasZones()) {
			import_tile->setZoneIds({});
		}

		(void)editor.map.setTile(new_pos, std::move(moved_tile));
	}

	for (auto& spawn_entry : spawn_map) {
		Position pos = spawn_entry.first;
		TileLocation* location = editor.map.createTileL(pos);
		Tile* tile = location->get();
		if (!tile) {
			tile = editor.map.createTile(pos.x, pos.y, pos.z);
		} else if (tile->spawn) {
			editor.map.removeSpawn(tile);
			tile->spawn.reset();
		}
		tile->spawn = std::move(spawn_entry.second);

		editor.map.addSpawn(tile);
	}

	if (editor.map.getVersion().otbm == MAP_OTBM_5) {
		editor.map.rebuildZoneIndex();
	}

	g_gui.DestroyLoadBar();

	editor.map.setWidth(newsize_x);
	editor.map.setHeight(newsize_y);
	DialogUtil::PopupDialog("Success", "Map imported successfully, " + i2ws(discarded_tiles) + " tiles were discarded as invalid.", wxOK);

	g_gui.RefreshPalettes();
	g_gui.FitViewToMap();

	return true;
}
bool EditorPersistence::importMiniMap(Editor& editor, FileName filename, int import, int import_x_offset, int import_y_offset, int import_z_offset) {
	return false;
}
