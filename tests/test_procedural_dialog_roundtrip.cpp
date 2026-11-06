// SPDX-License-Identifier: GPL-3.0-or-later
// Private application-entry probe. All commands after baseline creation come from the operator.
// Timing mode: the sampler thread reads only this process' memory counters, never RME/GUI objects.
#include "app/main.h"
#include "app/managers/version_manager.h"
#include "app/settings.h"
#include "brushes/ground/ground_brush.h"
#include "brushes/managers/brush_manager.h"
#include "editor/action_queue.h"
#include "editor/editor.h"
#include "editor/persistence/editor_persistence.h"
#include "procedural/adapter/snapshot_equal.h"
#include "procedural/adapter/terrain_preparation.h"
#include "ui/gui.h"
#include "ui/gui_ids.h"
#include "ui/main_frame.h"
#include "ui/main_menubar.h"

#include <wx/eventfilter.h>
#include <wx/module.h>
#include <wx/spinctrl.h>
#include <wx/timer.h>
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>

namespace {
namespace fs = std::filesystem;
using namespace rme::procedural;
using Clock = std::chrono::steady_clock;
const fs::path target = "generated/terrain.otbm";
constexpr size_t fixtureTiles = 5;
// Observed elapsed time ends at the first observer tick that sees the transition, so it is
// quantized to this period plus Windows timer resolution (about 15.6 ms).
constexpr int observerTickMs = 20;
constexpr auto samplerPeriod = std::chrono::milliseconds(5);
constexpr std::array<const char*, 4> timedOperations{"place", "undo", "redo", "save"};

void require(bool condition, const char* message) {
	if (!condition) throw std::runtime_error(message);
}
double millis(Clock::duration value) {
	return std::chrono::duration<double, std::milli>(value).count();
}
struct Memory {
	uint64_t workingSet = 0, privateBytes = 0;
};
Memory memory() {
	PROCESS_MEMORY_COUNTERS_EX counters{};
	require(GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)),
		"Process memory counters unavailable");
	return {counters.WorkingSetSize, counters.PrivateUsage};
}
struct Measurement {
	Memory baseline, peak;
	Clock::time_point start, lastSample;
	uint64_t samples = 0;
	double maxSampleGap = 0, maxHeartbeatGap = 0;
};

class RoundtripProbe final : public wxEvtHandler, public wxEventFilter {
public:
	RoundtripProbe() : timer(this), report("dialog-probe.log"),
		side(fs::is_regular_file(".g1-dialog-roundtrip-500") ? 500 : 100),
		timing(fs::is_regular_file(".g1-dialog-roundtrip-timing")) {
		report << std::unitbuf;
		wxString error;
		std::vector<std::string> warnings;
		require(g_version.LoadVersion("1098-BlackTek", error, warnings), "Private client load failed");
		g_settings.setInteger(Config::WAYPOINT_STORAGE_FORMAT, static_cast<int>(Config::WaypointStorageFormat::BOTH));
		// Both timing sizes use the same private Undo limit as the 500x500 round-trip.
		if (side == 500 || timing) g_settings.setInteger(Config::UNDO_MEM_SIZE, 512);
		require(g_gui.NewMap(), "Synthetic editor creation failed");
		auto* editor = g_gui.GetCurrentEditor();
		require(editor != nullptr, "Synthetic editor missing");
		createFixture(editor->map);
		fs::create_directory(target.parent_path());
		require(EditorPersistence::saveMap(*editor, FileName(wxString(fs::absolute(target).wstring())), false),
			"Baseline save failed");
		fs::copy(target.parent_path(), "baseline", fs::copy_options::recursive);
		editor->actionQueue->clear();
		editor->map.clearChanges();
		sourceGeneration = editor->map.getGeneration();
		if (!timing) {
			auto expected = prepareTerrain(editor->map, {{100, 100, 7, side, side}, 1729, 3, 2, 1},
				{{"grass", "dirt", "clear water"}}, {250000, 254016, 250000, 512ULL * 1024 * 1024});
			require(expected && expected->preview.getChanges().size() == size_t(side) * side, "Reference terrain failed");
			reference.emplace(std::move(expected->preview));
		}
		Bind(wxEVT_TIMER, &RoundtripProbe::tick, this);
		if (timing) startTiming();
		require(timer.Start(timing ? observerTickMs : 100), "Cannot start round-trip observer");
		report << "ROUNDTRIP_FIXTURE_READY\n";
	}
	~RoundtripProbe() override {
		timer.Stop();
		if (timing) wxEvtHandler::RemoveFilter(this);
	}

	// Timing starts when the real command event is dispatched, before its handler runs.
	int FilterEvent(wxEvent& event) override {
		if (!timing || timingDone) return Event_Skip;
		try {
			std::optional<int> operation;
			const auto type = event.GetEventType();
			if (type == wxEVT_BUTTON) {
				auto* control = dynamic_cast<wxWindow*>(event.GetEventObject());
				if (control && control->GetName() == "terrain-place") {
					for (const auto* field : {"terrain-width", "terrain-height"}) {
						auto* spin = dynamic_cast<wxSpinCtrl*>(wxWindow::FindWindowByName(field, control->GetParent()));
						require(spin && spin->GetValue() == int(side), "Timing Place requires dialog dimensions matching the round-trip side");
					}
					operation = 0;
				}
			} else if (type == wxEVT_MENU || type == wxEVT_TOOL) {
				const int id = event.GetId();
				require(id != wxID_UNDO && id != wxID_REDO && id != wxID_SAVE, "Timing mode requires Edit/File menu commands, not the toolbar");
				if (id == MAIN_FRAME_MENU + MenuBar::UNDO) operation = 1;
				else if (id == MAIN_FRAME_MENU + MenuBar::REDO) operation = 2;
				else if (id == MAIN_FRAME_MENU + MenuBar::SAVE) operation = 3;
			}
			if (!operation) return Event_Skip;
			bool measuring = false;
			{
				std::scoped_lock lock(metricsMutex);
				measuring = active;
			}
			if (measuring && *operation == phase) return Event_Skip; // Same command seen again while running.
			require(!measuring && *operation == phase, "Timing command out of order; use Place, Undo, Redo, Save");
			begin();
		} catch (const std::exception& error) {
			fail(error.what());
		}
		return Event_Skip;
	}

private:
	[[noreturn]] void fail(const char* message) {
		report << "FAILED\t" << message << '\n';
		csv.flush();
		std::_Exit(8);
	}

	void startTiming() {
		csv.open("roundtrip-timing.csv");
		require(csv.is_open(), "Cannot create timing CSV");
		csv << "order,operation,side,elapsed_observed_ms,observer_tick_ms,max_heartbeat_gap_ms,baseline_ws,peak_ws,end_ws,"
			   "baseline_private,peak_private,end_private,samples,max_sample_gap_ms\n";
		report << "ROUNDTRIP_TIMING_QUANTIZATION\tobserver_tick_ms=" << observerTickMs << "\tsampler_ms=" << samplerPeriod.count()
			   << "\telapsed ends at the first observer tick that sees the history/dirty transition\n";
		wxEvtHandler::AddFilter(this);
		lastTick = Clock::now();
		sampler = std::jthread([this](std::stop_token stop) {
			try {
				while (!stop.stop_requested()) {
					{
						std::scoped_lock lock(metricsMutex);
						if (active) sample();
					}
					std::this_thread::sleep_for(samplerPeriod);
				}
			} catch (...) {
				samplerFailed = true;
			}
		});
	}
	// Callers hold metricsMutex.
	void sample() {
		const auto now = Clock::now();
		const auto current = memory();
		metrics.peak.workingSet = (std::max)(metrics.peak.workingSet, current.workingSet);
		metrics.peak.privateBytes = (std::max)(metrics.peak.privateBytes, current.privateBytes);
		metrics.maxSampleGap = (std::max)(metrics.maxSampleGap, millis(now - metrics.lastSample));
		metrics.lastSample = now;
		++metrics.samples;
	}
	void begin() {
		std::scoped_lock lock(metricsMutex);
		const auto now = Clock::now();
		const auto current = memory();
		metrics = {current, current, now, now};
		active = true;
	}
	void finish() {
		Measurement result;
		Memory end;
		double elapsed = 0;
		{
			std::scoped_lock lock(metricsMutex);
			sample();
			active = false;
			end = memory();
			metrics.peak.workingSet = (std::max)(metrics.peak.workingSet, end.workingSet);
			metrics.peak.privateBytes = (std::max)(metrics.peak.privateBytes, end.privateBytes);
			result = metrics;
			elapsed = millis(Clock::now() - metrics.start);
		}
		const auto* operation = timedOperations[phase];
		csv << phase + 1 << ',' << operation << ',' << side << ',' << elapsed << ',' << observerTickMs << ','
			<< result.maxHeartbeatGap << ',' << result.baseline.workingSet << ',' << result.peak.workingSet << ','
			<< end.workingSet << ',' << result.baseline.privateBytes << ',' << result.peak.privateBytes << ','
			<< end.privateBytes << ',' << result.samples << ',' << result.maxSampleGap << '\n';
		csv.flush();
		report << "ROUNDTRIP_TIMING_ROW\t" << phase + 1 << '\t' << operation << '\t' << elapsed << '\n';
	}
	// Cheap safety checks for timing mode: O(1) counts and the five fixture tiles, no terrain scan.
	void checkCounts(const Map& map, bool present) const {
		require(map.getTileCount() == fixtureTiles + (present ? uint64_t(side) * side : 0), "Unexpected map tile count");
		checkFixture(map);
	}

	void createFixture(Map& map) {
		require(map.convert({MAP_OTBM_5, map.getVersion().client}), "Cannot configure synthetic OTBM 5 map");
		auto* brush = g_brushes.getBrush("grass");
		const auto* grass = brush ? brush->as<GroundBrush>() : nullptr;
		require(grass && !grass->getBorderItems().empty(), "Fixture ground unavailable");
		const auto groundId = grass->getBorderItems().front().id;
		const auto tileAt = [&](const Position& position) {
			auto* tile = map.createTile(position.x, position.y, position.z);
			tile->setGround(Item::Create(groundId));
			require(tile->ground != nullptr, "Fixture ground creation failed");
			return tile;
		};
		auto* houseTile = tileAt(housePosition);
		houseTile->ground->setUniqueID(32100);
		houseTile->ground->setActionID(32101);
		houseTile->ground->setText("G1 round-trip sentinel");
		auto house = std::make_unique<House>(map);
		house->setID(1);
		house->name = "G1 House";
		house->rent = 4321;
		house->setExit(housePosition);
		auto* houseRef = house.get();
		require(map.houses.addHouse(std::move(house)), "Fixture house creation failed");
		houseTile->setHouse(houseRef);

		auto* monster = tileAt(monsterPosition);
		monster->spawn = std::make_unique<Spawn>(2);
		monster->spawn->addKind(Spawn::MONSTERS);
		monster->creature = std::make_unique<Creature>("Rat");
		monster->creature->setSpawnTime(41);
		require(map.addSpawn(monster), "Fixture monster spawn failed");
		auto* npc = tileAt(npcPosition);
		npc->spawn = std::make_unique<Spawn>(3);
		npc->spawn->addKind(Spawn::NPCS);
		npc->creature = std::make_unique<Creature>("G1 Guide");
		npc->creature->markAsNpcSpawn();
		npc->creature->setSpawnTime(53);
		require(map.addSpawn(npc), "Fixture NPC spawn failed");

		tileAt(waypointPosition);
		map.waypoints.addWaypoint(std::make_unique<Waypoint>("G1 Waypoint", waypointPosition));
		auto* zoneTile = tileAt(zonePosition);
		zone = map.addZone("G1 Zone");
		require(zone != 0, "Fixture zone creation failed");
		zoneTile->addZone(zone);
		map.rebuildZoneIndex();
		checkFixture(map);
	}

	void checkFixture(const Map& map) const {
		const auto* house = map.houses.getHouse(1);
		const auto* tile = map.getTile(housePosition);
		require(house && house->name == "G1 House" && house->rent == 4321 && house->getExit() == housePosition &&
			tile && tile->getHouseID() == 1 && tile->ground && tile->ground->getUniqueID() == 32100 &&
			tile->ground->getActionID() == 32101 && tile->ground->getText() == "G1 round-trip sentinel", "House or item metadata changed");
		const auto checkSpawn = [&](const Position& position, int radius, const char* name, int spawnTime, bool isNpc) {
			const auto* spawnTile = map.getTile(position);
			require(spawnTile && spawnTile->spawn && spawnTile->spawn->getSize() == radius &&
				spawnTile->creature && spawnTile->creature->getName() == name && spawnTile->creature->isNpc() == isNpc &&
				spawnTile->creature->getSpawnTime() == spawnTime, "Spawn metadata changed");
		};
		checkSpawn(monsterPosition, 2, "Rat", 41, false);
		checkSpawn(npcPosition, 3, "G1 Guide", 53, true);
		const auto* waypoint = map.waypoints.getWaypoint(map.getTileL(waypointPosition));
		require(waypoint && waypoint->name == "G1 Waypoint" && waypoint->pos == waypointPosition, "Waypoint changed");
		const auto* zoneInfo = map.findZone("G1 Zone");
		require(zoneInfo && map.getZoneTilePositions(zone) == std::vector<Position>{zonePosition}, "Zone changed");
	}

	void checkTerrain(const Map& map, bool present) const {
		require(map.getTileCount() == fixtureTiles + (present ? reference->getChanges().size() : 0), "Unexpected map tile count");
		for (const auto& position : reference->getChanges()) {
			const auto* actual = map.getTile(position);
			const auto* expected = present ? reference->getBuffer().getTile(position) : nullptr;
			require(snapshotTileContentsEqual(actual, expected), "Terrain tile payload changed");
		}
		checkFixture(map);
	}

	// Finishes each row at the same history/dirty transitions as the normal round-trip.
	void tickTiming() {
		const auto now = Clock::now();
		const double gap = millis(now - lastTick);
		lastTick = now;
		bool measuring = false;
		{
			std::scoped_lock lock(metricsMutex);
			if (active) metrics.maxHeartbeatGap = (std::max)(metrics.maxHeartbeatGap, gap);
			measuring = active;
		}
		require(!samplerFailed, "Memory sampling failed");
		auto* editor = g_gui.GetCurrentEditor();
		require(editor && editor->map.getGeneration() == sourceGeneration, "Timing source editor changed");
		auto& map = editor->map;
		auto& history = *editor->actionQueue;
		const bool transitioned = (phase == 0 && history.canUndo()) || (phase == 1 && history.canRedo()) ||
			(phase == 2 && history.canUndo()) || (phase == 3 && !map.hasChanged());
		if (!transitioned) {
			if (phase == 0 && !measuring) require(history.getSize() == 0 && !map.hasChanged() && map.getTileCount() == fixtureTiles,
				"Source changed before Place");
			return;
		}
		require(measuring, "Transition without an observed Place button or menu command");
		finish();
		if (phase == 0) {
			require(history.getSize() == 1 && map.hasChanged(), "Place is not one dirty action");
			checkCounts(map, true);
			report << "ROUNDTRIP_PLACE_PASS\n";
		} else if (phase == 1) {
			require(!history.canUndo() && history.getSize() == 1, "Unexpected history after Undo");
			checkCounts(map, false);
			report << "ROUNDTRIP_UNDO_PASS\n";
		} else if (phase == 2) {
			require(history.getSize() == 1 && !history.canRedo(), "Unexpected history after Redo");
			checkCounts(map, true);
			report << "ROUNDTRIP_REDO_PASS\n";
		} else {
			require(history.getSize() == 1 && fs::is_regular_file(target), "Unexpected state after Save");
			checkCounts(map, true);
			report << "ROUNDTRIP_SAVED\n";
		}
		if (++phase < int(timedOperations.size())) return;
		timingDone = true;
		timer.Stop();
		sampler.request_stop();
		csv.close();
		report << "ROUNDTRIP_TIMING_PASS\n";
		// No reopening in timing mode. The operator closes the application normally.
	}

	void tick(wxTimerEvent&) {
		try {
			if (timing) {
				if (!timingDone) tickTiming();
				return;
			}
			// LoadMap can pump timer events while its modal import prompt is open.
			// Wait for the frame to be interactive before observing a reopened map.
			if (!g_gui.root->IsEnabled()) return;
			auto* editor = g_gui.GetCurrentEditor();
			if (phase == 4) {
				if (editor && editor->map.getGeneration() == sourceGeneration) return;
				++phase;
				report << "ROUNDTRIP_CLOSED\n";
			}
			if (!editor) return;
			auto& map = editor->map;
			auto& history = *editor->actionQueue;
			if (phase == 0 && history.canUndo()) {
				require(history.getSize() == 1 && map.hasChanged(), "Place is not one dirty action");
				checkTerrain(map, true);
				++phase;
				report << "ROUNDTRIP_PLACE_PASS\n";
			} else if (phase == 1 && history.canRedo()) {
				require(!history.canUndo() && history.getSize() == 1, "Unexpected history after Undo");
				checkTerrain(map, false);
				++phase;
				report << "ROUNDTRIP_UNDO_PASS\n";
			} else if (phase == 2 && history.canUndo()) {
				require(history.getSize() == 1 && !history.canRedo(), "Unexpected history after Redo");
				checkTerrain(map, true);
				++phase;
				report << "ROUNDTRIP_REDO_PASS\n";
			} else if (phase == 3 && !map.hasChanged()) {
				checkTerrain(map, true);
				++phase;
				report << "ROUNDTRIP_SAVED\n";
			} else if (phase == 5 && map.getGeneration() != sourceGeneration && map.getTileCount() > fixtureTiles) {
				require(map.getVersion().otbm == MAP_OTBM_5 && !map.hasChanged() && history.getSize() == 0,
					"Reopened map version, dirty state or history unexpected");
				checkTerrain(map, true);
				timer.Stop();
				report << "ROUNDTRIP_REOPEN_PASS\nROUNDTRIP_PASS\n";
				// Leave the reopened map visible. The operator closes the application normally.
			}
		} catch (const std::exception& error) {
			report << "FAILED\t" << error.what() << '\n';
			std::_Exit(8);
		}
	}

	wxTimer timer;
	std::ofstream report;
	const uint32_t side;
	const bool timing;
	const int fixtureOrigin = int(side) + 200;
	const Position housePosition{fixtureOrigin, fixtureOrigin, 7};
	const Position monsterPosition{fixtureOrigin + 10, fixtureOrigin, 7};
	const Position npcPosition{fixtureOrigin + 20, fixtureOrigin, 7};
	const Position zonePosition{fixtureOrigin + 30, fixtureOrigin, 8};
	const Position waypointPosition{fixtureOrigin + 40, fixtureOrigin, 7};
	std::optional<TerrainPreview> reference;
	uint64_t sourceGeneration = 0;
	uint16_t zone = 0;
	int phase = 0;
	std::ofstream csv;
	bool timingDone = false;
	Clock::time_point lastTick;
	std::mutex metricsMutex;
	Measurement metrics;
	bool active = false;
	std::atomic<bool> samplerFailed = false;
	// Destroyed first: stop/join before any state used by the sampler is destroyed.
	std::jthread sampler;
};

class RoundtripProbeModule final : public wxModule {
public:
	bool OnInit() override {
		if (fs::is_regular_file(".g1-dialog-roundtrip")) {
			wxTheApp->Bind(wxEVT_IDLE, &RoundtripProbeModule::idle, this);
		}
		return true;
	}
	void OnExit() override { probe.reset(); }
private:
	void idle(wxIdleEvent& event) {
		event.Skip();
		if (!g_gui.root || !wxApp::IsMainLoopRunning() || !g_gui.root->IsEnabled() || probe) return;
		try {
			probe = std::make_unique<RoundtripProbe>();
		} catch (const std::exception& error) {
			std::ofstream("dialog-probe.log", std::ios::app) << "FAILED\t" << error.what() << '\n';
			std::_Exit(8);
		}
	}
	std::unique_ptr<RoundtripProbe> probe;
	wxDECLARE_DYNAMIC_CLASS(RoundtripProbeModule);
};
wxIMPLEMENT_DYNAMIC_CLASS(RoundtripProbeModule, wxModule);
}
