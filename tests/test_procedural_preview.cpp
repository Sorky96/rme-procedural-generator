// SPDX-License-Identifier: GPL-3.0-or-later
// Private loaded-client wxModule probe, linked with the current real RME.
#include "app/main.h"
#include "editor/operations/draw_operations.h"
#include "procedural/adapter/generation_snapshot.h"
#include "procedural/adapter/ground_resolver.h"
#include "procedural/adapter/snapshot_equal.h"
#include "procedural/adapter/terrain_preparation.h"
#include "procedural/adapter/generation_selection.h"
#include "procedural/adapter/terrain_validation.h"
#include "procedural/adapter/generation_assets.h"
#include "procedural/adapter/terrain_target.h"
#include "editor/editor_tabs.h"
#include "ui/map_tab.h"
#include "live/live_client.h"
#include "editor/managers/editor_manager.h"
#include "brushes/ground/auto_border.h"
#include "rendering/core/graphics.h"
#include "rendering/core/game_sprite.h"
#include "app/managers/version_manager.h"
#include "brushes/ground/ground_brush.h"
#include "brushes/ground/ground_border_calculator.h"
#include "editor/editor.h"
#include "app/settings.h"
#include "ui/managers/minimap_manager.h"
#include "editor/action_queue.h"
#include "lua/lua_engine.h"
#include "lua/lua_script_manager.h"
#include "map/map.h"
#include "map/tile_operations.h"
#include "ui/gui.h"
#include "ui/main_frame.h"
#include <wx/module.h>
#include <chrono>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <new>
#include <thread>
#include <type_traits>

namespace {
thread_local bool recording = false;
thread_local size_t ordinal = 0, failAt = 0;
thread_local bool observed = false;

// Fixed, nonallocating test ledger. malloc pointers keep their normal layout:
// adding allocation headers would break wx/DLL cross-module new/delete pairs.
struct HeapEntry { void* pointer; size_t bytes; uint64_t epoch; };
constexpr size_t heapSlots = size_t{1} << 21;
HeapEntry heapEntries[heapSlots] {};
uint64_t heapEpoch = 0, heapLive = 0, heapPeak = 0;
bool heapOverflow = false;
thread_local bool heapOwner = false, heapRecording = false;

size_t heapIndex(void* pointer) noexcept {
	return size_t(((uint64_t(reinterpret_cast<uintptr_t>(pointer)) >> 4) * 11400714819323198485ULL) >> (64-21));
}
void recordHeapAllocation(void* pointer, size_t bytes) noexcept {
	if (!heapRecording) return;
	size_t tombstone = heapSlots;
	for (size_t probe=0, index=heapIndex(pointer); probe<heapSlots; ++probe, index=(index+1)&(heapSlots-1)) {
		auto& entry = heapEntries[index];
		if (entry.epoch == heapEpoch && entry.pointer == pointer) {
			// A DLL may have freed a tracked pointer without passing our delete.
			// Reuse of the address proves that allocation is no longer live.
			heapLive -= entry.bytes;
			entry.bytes = bytes;
			heapLive += bytes;
			heapPeak = std::max(heapPeak,heapLive);
			return;
		}
		if (entry.epoch != heapEpoch) {
			heapEntries[tombstone == heapSlots ? index : tombstone] = {pointer,bytes,heapEpoch};
			heapLive += bytes;
			heapPeak = std::max(heapPeak,heapLive);
			return;
		}
		if (!entry.pointer && tombstone == heapSlots) tombstone = index;
	}
	heapOverflow = true;
}
void recordHeapDeallocation(void* pointer) noexcept {
	if (!heapOwner || !pointer) return;
	for (size_t probe=0, index=heapIndex(pointer); probe<heapSlots; ++probe, index=(index+1)&(heapSlots-1)) {
		auto& entry = heapEntries[index];
		if (entry.epoch != heapEpoch) return;
		if (entry.pointer == pointer) {
			heapLive -= entry.bytes;
			entry.pointer = nullptr;
			return;
		}
	}
}
void startHeapMeasurement() noexcept {
	++heapEpoch;
	heapLive = heapPeak = 0;
	heapOverflow = false;
	heapOwner = heapRecording = true;
}
}
void* operator new(size_t size) {
	if (recording && ++ordinal == failAt) { observed = true; throw std::bad_alloc(); }
	if (auto* value = std::malloc(size ? size : 1)) {
		recordHeapAllocation(value,size);
		return value;
	}
	throw std::bad_alloc();
}
void* operator new[](size_t size) { return ::operator new(size); }
void operator delete(void* value) noexcept { recordHeapDeallocation(value); std::free(value); }
void operator delete(void* value, size_t) noexcept { ::operator delete(value); }
void operator delete[](void* value) noexcept { ::operator delete(value); }
void operator delete[](void* value, size_t) noexcept { ::operator delete(value); }

std::mt19937& getRandomGenerator();
namespace {
using namespace rme::procedural;
constexpr CoreBudget coreBudget {250000, 1024*1024};
constexpr SnapshotBudget snapshotBudget {254016, 256ULL*1024*1024};
constexpr PreviewBudget previewBudget {250000, 512ULL*1024*1024};
constexpr PreparationBudget preparationBudget {250000,254016,250000,512ULL*1024*1024};
const GroundProfile profile {{{"grass", "dirt", "clear water"}}};
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }

std::array<Editor*,2> observedEditors {};
const Position markerPosition(900,900,7);

// Read cache invalidation through public bounds, with no shipping test hooks.
// The temporary coordinate is restored before checking/throwing or yielding.
void selectionCacheState(Editor& editor, bool invalidated = false) {
	auto& selection = editor.selection;
	auto* tile = editor.map.getTile(markerPosition);
	check(tile && selection.size() == 1 && selection.getSelectedTile() == tile && !selection.isBusy(), "Selection oracle fixture changed");
	auto* location = tile->getLocation();
	const Position alternate(901,902,7);
	Position minimum, maximum;
	location->setPosition(alternate);
	try {
		minimum = selection.minPosition();
		maximum = selection.maxPosition();
	} catch (...) {
		location->setPosition(markerPosition);
		throw;
	}
	location->setPosition(markerPosition);
	selection.markChanged();
	check(selection.minPosition() == markerPosition && selection.maxPosition() == markerPosition, "Selection oracle reset failed");
	selection.start(Selection::INTERNAL);
	selection.finish(Selection::INTERNAL);
	const auto expected = invalidated ? alternate : markerPosition;
	check(minimum == expected && maximum == expected, "Active/inactive editor selection cache was unexpectedly invalidated");
}

void selectionCachesUnchanged() {
	for (auto* editor : observedEditors) {
		check(editor != nullptr, "Actual editor observation missing");
		selectionCacheState(*editor);
	}
}

void lua(const std::string& code) {
	auto& engine = g_luaScripts.getEngine();
	if (!engine.executeString(code,"g1-selection-isolation")) throw std::runtime_error(engine.getLastError());
}

template<typename Operation>
void selectionEvent(Operation&& operation, bool expected = false) {
	auto* editor = g_gui.GetCurrentEditor();
	check(editor && g_luaScripts.isInitialized(), "Actual editor/Lua observer missing");
	lua("g1_selection_notifications = 0");
	editor->selection.start();
	operation();
	editor->selection.finish();
	lua(expected ? "assert(g1_selection_notifications == 1)" : "assert(g1_selection_notifications == 0)");
}

void createObservers(std::ostream& report) {
	const auto* water = g_brushes.getBrush("clear water")->as<GroundBrush>();
	check(water && !water->getBorderItems().empty(), "Observer water brush missing");
	for (auto& editor : observedEditors) {
		check(g_gui.NewMap(), "Private editor creation failed");
		editor = g_gui.GetCurrentEditor();
		check(editor != nullptr, "Created editor is not active");
		auto* tile = editor->map.createTile(markerPosition.x,markerPosition.y,markerPosition.z);
		tile->setGround(Item::Create(water->getBorderItems().front().id));
		tile->ground->select();
		TileOperations::updateSelectionState(tile);
		editor->selection.start(Selection::INTERNAL);
		editor->selection.addInternal(tile);
		editor->selection.finish(Selection::INTERNAL);
		check(editor->selection.minPosition() == markerPosition && editor->selection.maxPosition() == markerPosition, "Observer bounds missing");
	}
	check(observedEditors[0] != observedEditors[1] && g_gui.GetCurrentEditor() == observedEditors[1], "Two real editors required");
	check(g_luaScripts.isInitialized(), "Normal application Lua engine missing");
	lua("g1_selection_notifications = 0; g1_selection_listener = app.events:on('selectionChange', function() g1_selection_notifications = g1_selection_notifications + 1 end)");
	// Positive controls: prove the public oracle sees actual dirty flags/events.
	auto* live = observedEditors[1]->map.getTile(markerPosition);
	selectionEvent([&] { TileOperations::markSelectionChanged(live); },true);
	selectionCacheState(*observedEditors[1],true);
	live->ground->deselect();
	TileOperations::updateSelectionState(live);
	selectionCacheState(*observedEditors[1],true);
	live->ground->select();
	TileOperations::updateSelectionState(live);
	selectionCacheState(*observedEditors[1],true);
	live->modify();
	selectionCacheState(*observedEditors[1],true);
	selectionCachesUnchanged();
	report << "SELECTION_OBSERVERS_PASS\t2\n";
}

void detachedNotifications(std::ostream& report) {
	auto* live = observedEditors[1]->map.getTile(markerPosition);
	auto copy = live->deepCopy();
	auto actionCopy = TileOperations::deepCopy(live,observedEditors[1]->map);
	selectionEvent([&] {
		copy->ground->deselect();
		TileOperations::updateSelectionState(copy.get());
		TileOperations::markSelectionChanged(copy.get());
		actionCopy->ground->deselect();
		TileOperations::updateSelectionState(actionCopy.get());
		TileOperations::markSelectionChanged(observedEditors[0]->map.getTile(markerPosition));
		TileOperations::markSelectionChanged(nullptr);
		Tile invalid(-1,-1,-1);
		TileOperations::markSelectionChanged(&invalid);
	});
	selectionCachesUnchanged();
	report << "SELECTION_DETACHED_NOTIFICATIONS_PASS\n";
}

GroundPlan resolve(const GenerationSnapshot& snapshot) {
	auto grid = generateTerrain(snapshot.input.config, snapshot.input.extent, coreBudget,
		snapshot.input.mask, snapshot.input.protection);
	check(bool(grid), "Semantic generation failed");
	auto plan = resolveGroundPlan(snapshot.input, *grid, profile, 1024*1024);
	check(bool(plan), "Resolver failed");
	return std::move(*plan);
}

void oneSize(uint32_t side, bool mixed, std::ostream& report) {
	Map map;
	map.initializeEmpty();
	const GenerationConfig config {{100,100,7,side,side}, 1729, mixed ? 70u : 0u, mixed ? 20u : 0u, mixed ? 10u : 1u};
	const auto snapshot = captureGenerationSnapshot(map, config, coreBudget, snapshotBudget);
	check(bool(snapshot), "Snapshot failed");
	const auto plan = resolve(*snapshot);
	const auto randomBefore = getRandomGenerator();
	const auto started = std::chrono::steady_clock::now();
	const auto preview = DrawOperations::prepareTerrainPreview(map,*snapshot,plan,previewBudget);
	const auto elapsed = std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
	if (!preview) report << "PREVIEW_ERROR\t" << side << '\t' << mixed << '\t' << int(preview.error()) << '\n';
	check(bool(preview), "Valid preview rejected");
	check(preview->getChanges().size() == size_t(side)*side, "Preview lost generated cells");
	check(preview->currentFingerprint() == preview->getFingerprint(), "Fresh fingerprint invalid");
	const auto repeat = DrawOperations::prepareTerrainPreview(map,*snapshot,plan,previewBudget);
	check(repeat && repeat->getFingerprint() == preview->getFingerprint(), "Preview not deterministic");
	size_t borders = 0;
	for (const auto& position : preview->getChanges()) {
		const auto* tile = preview->getBuffer().getTile(position);
		check(tile && snapshotTilesEqual(tile,repeat->getBuffer().getTile(position)), "Repeated concrete tile differs");
		borders += tile->items.size();
	}
	if (mixed) check(borders > 0, "Mixed fixture did not exercise borders");
	check(getRandomGenerator() == randomBefore, "Sandbox advanced global RNG");
	check(map.getTileCount() == 0 && snapshotMatchesMap(map,*snapshot), "Sandbox mutated source");
	if (side == 500 && mixed) {
		std::stop_source during;
		std::jthread canceller([&] {
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			during.request_stop();
		});
		const auto stopped = DrawOperations::prepareTerrainPreview(map,*snapshot,plan,previewBudget,during.get_token());
		canceller.join();
		check(!stopped && stopped.error() == PreviewError::Cancelled && snapshotMatchesMap(map,*snapshot), "Mid-call Cancel changed source or returned output");
	}
	const auto exact = DrawOperations::prepareTerrainPreview(map,*snapshot,plan,{previewBudget.maxChanges,preview->getEstimatedPeakBytes()});
	check(exact && exact->getFingerprint() == preview->getFingerprint(), "Exact preview estimate rejected");
	const auto shortBudget = DrawOperations::prepareTerrainPreview(map,*snapshot,plan,{previewBudget.maxChanges,preview->getEstimatedPeakBytes()-1});
	check(!shortBudget && shortBudget.error() == PreviewError::Limit, "Short preview budget accepted");
	report << "PREVIEW_SIZE_PASS\t" << side << '\t' << mixed << '\t' << borders << '\t' << elapsed << '\t'
		<< preview->getEstimatedPeakBytes() << '\t' << preview->getFingerprint() << '\n';
	// const BaseMap exposes borrowed mutable Item pointers. Reject payload changes
	// rather than allowing a cached ID-only fingerprint to hide them.
	preview->getBuffer().getTile(preview->getChanges().front())->ground->setUniqueID(7001);
	check(!preview->currentFingerprint(), "Tampered attributes hidden by preview fingerprint");
	selectionCachesUnchanged();
}

void failures(std::ostream& report) {
	Tile before(1,1,7), cacheChanged(1,1,7);
	cacheChanged.statflags |= TILESTATE_BLOCKING;
	cacheChanged.minimapColor = 0;
	check(!snapshotTilesEqual(&before,&cacheChanged) && snapshotTileContentsEqual(&before,&cacheChanged), "Derived-cache diff policy wrong");
	cacheChanged.statflags |= 0x8000;
	check(!snapshotTileContentsEqual(&before,&cacheChanged), "Unknown statflag ignored by diff");
	Map map;
	map.initializeEmpty();
	const GenerationConfig config {{100,100,7,4,4},1729,0,0,1};
	// A copied but unaffected halo payload must survive every failure unchanged.
	map.createTile(98,98,7)->mapflags = TILESTATE_PROTECTIONZONE;
	map.createTile(100,100,7)->statflags = TILESTATE_SELECTED;
	auto snapshot = captureGenerationSnapshot(map,config,coreBudget,snapshotBudget);
	check(bool(snapshot), "Failure snapshot failed");
	auto plan = resolve(*snapshot);
	const auto baseline = DrawOperations::prepareTerrainPreview(map,*snapshot,plan,previewBudget);
	check(bool(baseline), "Failure fixture preview failed");
	ordinal = 0; failAt = SIZE_MAX; recording = true;
	const auto measured = DrawOperations::prepareTerrainPreview(map,*snapshot,plan,previewBudget);
	recording = false;
	check(bool(measured), "Allocation baseline failed");
	const auto allocations = ordinal;
	for (size_t i = 1; i <= allocations; ++i) {
		ordinal = 0; failAt = i; observed = false; recording = true;
		const auto result = DrawOperations::prepareTerrainPreview(map,*snapshot,plan,previewBudget);
		recording = false;
		check(observed, "Allocation fault was not reached");
		// stable_partition may intentionally fall back if its temporary allocation
		// fails. A complete, identical result is acceptable in that case.
		check((!result && result.error() == PreviewError::AllocationFailed) ||
			(result && result->getFingerprint() == baseline->getFingerprint()), "Allocation fault returned partial/wrong preview");
		check(snapshotMatchesMap(map,*snapshot), "Allocation failure changed source");
		selectionCachesUnchanged();
	}
	std::stop_source cancel;
	cancel.request_stop();
	const auto cancelled = DrawOperations::prepareTerrainPreview(map,*snapshot,plan,previewBudget,cancel.get_token());
	check(!cancelled && cancelled.error() == PreviewError::Cancelled, "Pre-cancelled sandbox ran");
	bool rejected = false;
	std::jthread worker([&] {
		const auto result = DrawOperations::prepareTerrainPreview(map,*snapshot,plan,previewBudget);
		rejected = !result && result.error() == PreviewError::WrongThread;
	});
	worker.join();
	check(rejected, "Worker touched live assets");
	const auto limited = DrawOperations::prepareTerrainPreview(map,*snapshot,plan,{15,previewBudget.maxBytes});
	check(!limited && limited.error() == PreviewError::Limit, "Change count limit ignored");
	auto invalid = plan;
	invalid.items[0] = 0;
	const auto badPlan = DrawOperations::prepareTerrainPreview(map,*snapshot,invalid,previewBudget);
	check(!badPlan && badPlan.error() == PreviewError::InvalidPlan, "Missing placement accepted");
	invalid = plan;
	invalid.items[0] = UINT16_MAX;
	const auto badItem = DrawOperations::prepareTerrainPreview(map,*snapshot,invalid,previewBudget);
	check(!badItem && badItem.error() == PreviewError::InvalidItem, "Invalid concrete item accepted");
	invalid = plan;
	invalid.profile.brushes[0] = "__missing_preview_brush__";
	const auto badProfile = DrawOperations::prepareTerrainPreview(map,*snapshot,invalid,previewBudget);
	check(!badProfile && badProfile.error() == PreviewError::InvalidPlan, "Missing profile silently substituted");
	++snapshot->context.x;
	const auto badContext = DrawOperations::prepareTerrainPreview(map,*snapshot,plan,previewBudget);
	check(!badContext, "Forged snapshot geometry accepted");
	--snapshot->context.x;
	map.getTile(98,98,7)->mapflags |= TILESTATE_NOLOGOUT;
	const auto stale = DrawOperations::prepareTerrainPreview(map,*snapshot,plan,previewBudget);
	check(!stale && stale.error() == PreviewError::StaleSnapshot, "Untracked raw mutation accepted");
	report << "PREVIEW_FAILURES_PASS\t" << allocations << '\n';
}

void excluded(std::ostream& report) {
	// Grass next to protected water requires its outer border on that water.
	// Reject all, preserving the outside payload, rather than silently clipping.
	Map map;
	map.initializeEmpty();
	GenerationConfig config {{100,100,7,8,8},1729,1,0,0};
	const auto* water = g_brushes.getBrush("clear water")->as<GroundBrush>();
	check(water && !water->getBorderItems().empty(), "Water fixture brush missing");
	auto* protectedTile = map.createTile(104,104,7);
	protectedTile->ground = Item::Create(water->getBorderItems().front().id);
	protectedTile->ground->setUniqueID(7001);
	protectedTile->ground->setActionID(6001);
	protectedTile->mapflags = TILESTATE_PROTECTIONZONE;
	const auto snapshot = captureGenerationSnapshot(map,config,coreBudget,snapshotBudget);
	check(bool(snapshot), "Excluded snapshot failed");
	const auto plan = resolve(*snapshot);
	const auto result = DrawOperations::prepareTerrainPreview(map,*snapshot,plan,previewBudget);
	if (result) report << "EXCLUDED_UNEXPECTED_SUCCESS\n";
	check(!result && result.error() == PreviewError::OutOfMask, "Out-of-mask shore was clipped or accepted");
	check(snapshotMatchesMap(map,*snapshot), "Excluded preview changed protected data");
	report << "PREVIEW_EXCLUDED_PASS\n";
}

void irregularAndEpoch(std::ostream& report) {
	Map map;
	map.initializeEmpty();
	const GenerationConfig config {{100,100,7,8,8},1729,0,0,1};
	std::vector<uint8_t> mask(64,1);
	for (size_t i=0; i<mask.size(); i+=3) mask[i]=0;
	map.createTileL(104,104,7)->increaseTownCount();
	const auto snapshot = captureGenerationSnapshot(map,config,coreBudget,snapshotBudget,mask);
	check(bool(snapshot), "Irregular capture failed");
	const auto plan = resolve(*snapshot);
	const auto preview = DrawOperations::prepareTerrainPreview(map,*snapshot,plan,previewBudget);
	check(preview && snapshotMatchesMap(map,*snapshot), "Irregular water preview failed/mutated source");
	size_t expected = 0;
	for (size_t i=0; i<mask.size(); ++i) {
		const bool writable = mask[i] && !snapshot->input.protection[i];
		expected += writable;
		check(bool(preview->getBuffer().getTile(100+int(i%8),100+int(i/8),7)) == writable, "Preview escaped exact irregular mask/protection");
	}
	check(preview->getChanges().size() == expected, "Irregular diff count wrong");
	const auto prepared = prepareTerrain(map,config,profile,preparationBudget,mask);
	check(prepared && validatePreparedTerrain(map,*prepared,config,profile,mask), "Epoch validation fixture failed");
	wxString error;
	std::vector<std::string> warnings;
	g_version.UnloadVersion();
	const auto missing = validatePreparedTerrain(map,*prepared,config,profile,mask);
	check(!missing && missing.error() == PreparedValidationError::NoClient, "Prepared validation accepted unloaded client");
	check(g_version.LoadVersion("1098-BlackTek",error,warnings,true), "Epoch reload failed");
	const auto stalePrepared = validatePreparedTerrain(map,*prepared,config,profile,mask);
	check(!stalePrepared && stalePrepared.error() == PreparedValidationError::StaleAssets, "Prepared validation accepted reloaded assets");
	report << "PREPARED_VALIDATION_EPOCH_PASS\n";
	check(!preview->currentFingerprint(), "Stale preview touched reloaded asset pointers");
	const auto stale = DrawOperations::prepareTerrainPreview(map,*snapshot,plan,previewBudget);
	check(!stale && stale.error() == PreviewError::StaleSnapshot, "Stale asset snapshot accepted");
	report << "PREVIEW_MASK_EPOCH_PASS\t" << expected << '\n';
}

void selectedContext(std::ostream& report) {
	const GenerationConfig config {{100,100,7,4,4},1729,0,0,1};
	const auto* water = g_brushes.getBrush("clear water")->as<GroundBrush>();
	for (auto* editor : observedEditors) {
		auto& map = editor->map;
		map.createTile(100,100,7)->statflags = TILESTATE_SELECTED;
		auto* neighbour = map.createTile(99,100,7);
		neighbour->ground = Item::Create(water->getBorderItems().front().id);
		neighbour->ground->select();
		neighbour->statflags = TILESTATE_SELECTED;
		const auto snapshot = captureGenerationSnapshot(map,config,coreBudget,snapshotBudget);
		check(bool(snapshot), "Selected context capture failed");
		const auto plan = resolve(*snapshot);
		selectionEvent([&] {
			const auto result = DrawOperations::prepareTerrainPreview(map,*snapshot,plan,previewBudget);
			check(result && result->getChanges().size() == 16 && snapshotMatchesMap(map,*snapshot), "Selected context preview failed or changed source");
			check(!result->getBuffer().getTile(100,100,7)->isSelected() && map.getTile(100,100,7)->isSelected(),
				"Empty source selection bit was changed or copied as a stale generated selection");
			check(!result->getBuffer().getTile(99,100,7), "Unchanged selected halo leaked into diff");
		});
		selectionCachesUnchanged();
	}
	report << "PREVIEW_SELECTED_CONTEXT_ISOLATION_PASS\t2\n";
}

void selectedBorderRefusal(std::ostream& report) {
	Map source;
	source.initializeEmpty();
	const auto* water = g_brushes.getBrush("clear water")->as<GroundBrush>();
	const auto* grass = g_brushes.getBrush("grass")->as<GroundBrush>();
	const Position center(104,104,7);
	BaseMap fixture;
	for (int y=103; y<=105; ++y) for (int x=103; x<=105; ++x) {
		fixture.createTile(x,y,7)->setGround(Item::Create((x==104 && y==104 ? water : grass)->getBorderItems().front().id));
	}
	auto* tile = fixture.getTile(center);
	GroundBorderCalculator::calculate(&fixture,tile,true);
	check(!tile->items.empty(), "Selected border fixture missing real borders");
	tile->items.front()->select();
	tile->statflags |= TILESTATE_SELECTED;
	tile->ground->setUniqueID(7001);
	tile->ground->setActionID(6001);
	(void)source.setTile(TileOperations::deepCopy(tile,source));
	const GenerationConfig config {{100,100,7,8,8},1729,1,0,0};
	const auto snapshot = captureGenerationSnapshot(source,config,coreBudget,snapshotBudget);
	check(bool(snapshot), "Selected border capture failed");
	const auto plan = resolve(*snapshot);
	selectionEvent([&] {
		const auto result = DrawOperations::prepareTerrainPreview(source,*snapshot,plan,previewBudget);
		check(!result && result.error() == PreviewError::OutOfMask && snapshotMatchesMap(source,*snapshot), "Selected border change was clipped/accepted or changed source");
	});
	selectionCachesUnchanged();
	auto* original = source.getTile(center);
	original->items.front()->deselect();
	original->statflags &= ~TILESTATE_SELECTED;
	const auto unselected = captureGenerationSnapshot(source,config,coreBudget,snapshotBudget);
	check(bool(unselected), "Unselected border control capture failed");
	const auto unselectedPlan = resolve(*unselected);
	selectionEvent([&] {
		const auto result = DrawOperations::prepareTerrainPreview(source,*unselected,unselectedPlan,previewBudget);
		check(result && result->getChanges().size() == 63 && snapshotMatchesMap(source,*unselected),
			"Unselected identical-border positive control failed");
		check(!result->getBuffer().getTile(center), "Unchanged protected border leaked into diff");
	});
	selectionCachesUnchanged();
	report << "PREVIEW_SELECTED_BORDER_REFUSAL_PASS\n";
}

void liveSelectionAndLua(std::ostream& report) {
	auto* editor = g_gui.GetCurrentEditor();
	check(editor == observedEditors[1], "Live selection test lost current editor");
	auto* tile = editor->map.getTile(markerPosition);
	editor->selection.start();
	editor->selection.remove(tile);
	editor->selection.finish();
	check(editor->selection.empty() && editor->selection.minPosition() == Position(), "Normal deselection failed");
	check(g_gui.DoUndo() && editor->selection.size() == 1 && editor->selection.minPosition() == markerPosition,
		"Normal selection Undo/bounds failed");
	check(editor->selection.getSelectedTile() == editor->map.getTile(markerPosition), "Selection Undo retained displaced pointer");
	check(g_gui.DoRedo() && editor->selection.empty(), "Normal selection Redo failed");
	check(g_gui.DoUndo() && editor->selection.size() == 1 && editor->selection.minPosition() == markerPosition,
		"Normal selection final Undo failed");
	lua("app.map:getTile(900,900,7).ground.actionId = 6001");
	check(editor->map.getTile(markerPosition)->ground->getActionID() == 6001 &&
		editor->selection.getSelectedTile() == editor->map.getTile(markerPosition), "Lua setter lost selected tile identity");
	check(g_gui.DoUndo() && editor->map.getTile(markerPosition)->ground->getActionID() == 0, "Lua selected setter Undo failed");
	check(g_gui.DoRedo() && editor->map.getTile(markerPosition)->ground->getActionID() == 6001, "Lua selected setter Redo failed");
	check(g_gui.DoUndo() && editor->selection.getSelectedTile() == editor->map.getTile(markerPosition) &&
		editor->selection.minPosition() == markerPosition && editor->selection.maxPosition() == markerPosition,
		"Lua final Undo lost selected bounds/pointer");
	for (auto* observedEditor : observedEditors) {
		observedEditor->selection.markChanged();
		check(observedEditor->selection.minPosition() == markerPosition, "Final observer reset failed");
	}
	selectionCachesUnchanged();
	report << "SELECTION_LIVE_LUA_UNDO_REDO_PASS\n";
}

void closeObservers() {
	lua("assert(app.events:off(g1_selection_listener)); g1_selection_listener = nil; g1_selection_notifications = nil");
	for (auto* editor : observedEditors) {
		editor->actionQueue->clear();
		editor->map.clearChanges();
	}
	observedEditors = {};
	check(g_gui.CloseAllEditors(), "Private fixture editors did not close");
}

template<typename Error>
bool failedAs(const std::expected<PreparedTerrain,PreparationError>& result, Error error) {
	const auto* actual = result ? nullptr : std::get_if<Error>(&result.error());
	return actual && *actual == error;
}

void preparationSize(uint32_t side, std::ostream& report) {
	Map map;
	map.initializeEmpty();
	const GenerationConfig config {{100,100,7,side,side},1729};
	uint64_t expectedFingerprint = 0, isolatedPreviewBytes = 0;
	std::vector<uint16_t> expectedItems;
	{
		const auto snapshot = captureGenerationSnapshot(map,config,coreBudget,snapshotBudget);
		check(bool(snapshot), "Preparation reference capture failed");
		const auto plan = resolve(*snapshot);
		const auto preview = DrawOperations::prepareTerrainPreview(map,*snapshot,plan,previewBudget);
		check(bool(preview), "Preparation reference preview failed");
		expectedFingerprint = preview->getFingerprint();
		isolatedPreviewBytes = preview->getEstimatedPeakBytes();
		expectedItems = plan.items;
	}
	const auto randomBefore = getRandomGenerator();
	uint64_t estimate = 0, measuredPeak = 0;
	startHeapMeasurement();
	{
		auto result = prepareTerrain(map,config,profile,preparationBudget);
		heapRecording = false;
		measuredPeak = heapPeak;
		check(bool(result), "Joint preparation failed");
		estimate = result->memory.totalBytes;
		const auto& memory = result->memory;
		check(!heapOverflow && measuredPeak <= estimate && estimate <= preparationBudget.maxBytes, "Measured new peak exceeded joint estimate or ledger overflowed");
		check(estimate == memory.infrastructureBytes + memory.snapshotBytes + memory.semanticBytes + memory.planBytes + memory.previewBytes,
			"Preparation memory charges do not add up");
		check(memory.snapshotBytes == result->snapshot.estimatedBytes && memory.previewBytes == result->preview.getEstimatedPeakBytes(),
			"Preparation changed stage estimates");
		check(result->plan.items == expectedItems && result->preview.getFingerprint() == expectedFingerprint &&
			result->preview.currentFingerprint() == expectedFingerprint && snapshotMatchesMap(map,result->snapshot),
			"Joint wrapper changed deterministic output/source");
		PreparedTerrain moved(std::move(*result));
		check(!result->preview.currentFingerprint() && moved.preview.currentFingerprint() == expectedFingerprint &&
			snapshotMatchesMap(map,moved.snapshot), "Prepared owners did not survive move");
		ordinal = 0; failAt = 1; observed = false; recording = true;
		const auto validated = validatePreparedTerrain(map,moved,config,profile);
		recording = false;
		check(validated && ordinal == 0 && !observed, "Prepared validation allocated or rejected fresh result");
		const auto movedFrom = validatePreparedTerrain(map,*result,config,profile);
		check(!movedFrom, "Moved-from prepared result validated");
		report << "PREPARED_VALIDATION_SIZE_PASS\t" << side << "\t0\t" << expectedFingerprint << '\n';
		auto exactBudget = preparationBudget;
		exactBudget.maxBytes = estimate;
		const auto exact = prepareTerrain(map,config,profile,exactBudget);
		check(exact && exact->memory.totalBytes == estimate && exact->preview.getFingerprint() == expectedFingerprint, "Exact joint budget failed");
		--exactBudget.maxBytes;
		const auto shortBudget = prepareTerrain(map,config,profile,exactBudget);
		check(failedAs(shortBudget,PreviewError::Limit), "One-byte-short joint budget accepted or lost stage failure");
		// Each individual phase fitted the isolated preview limit in the reference
		// pipeline. Simultaneously owned snapshot/core/plan must also be charged.
		check(memory.snapshotBytes < isolatedPreviewBytes && memory.semanticBytes < isolatedPreviewBytes && memory.planBytes < isolatedPreviewBytes,
			"Joint-overlap fixture does not fit isolated phases");
		exactBudget.maxBytes = isolatedPreviewBytes;
		const auto overlap = prepareTerrain(map,config,profile,exactBudget);
		check(failedAs(overlap,PreviewError::Limit), "Independent per-stage budgets bypassed joint ownership limit");
		check(map.getTileCount() == 0 && snapshotMatchesMap(map,moved.snapshot), "Joint limit failure changed source");
	}
	const auto retained = heapLive;
	heapOwner = false;
	check(!heapOverflow && retained <= preparationScratchBytes, "Preparation retained unexpected tracked allocations after destruction");
	check(getRandomGenerator() == randomBefore, "Preparation changed global RNG");
	selectionCachesUnchanged();
	if (side == 500) {
		std::stop_source during;
		std::jthread canceller([&] { std::this_thread::sleep_for(std::chrono::milliseconds(10)); during.request_stop(); });
		const auto stopped = prepareTerrain(map,config,profile,preparationBudget,{},during.get_token());
		canceller.join();
		check(!stopped && std::visit([](auto error) { return error == decltype(error)::Cancelled; },stopped.error()) && map.getTileCount() == 0,
			"Joint mid-call cancellation returned output or changed source");
		selectionCachesUnchanged();
	}
	report << "PREPARATION_SIZE_PASS\t" << side << '\t' << estimate << '\t' << measuredPeak << '\t' << retained << '\t' << expectedFingerprint << '\n';
}

void preparationFailures(std::ostream& report) {
	Map map;
	map.initializeEmpty();
	map.createTile(98,98,7)->mapflags = TILESTATE_PROTECTIONZONE;
	map.createTile(100,100,7)->statflags = TILESTATE_SELECTED;
	const GenerationConfig config {{100,100,7,4,4},1729,0,0,1};
	const auto source = captureGenerationSnapshot(map,config,coreBudget,snapshotBudget);
	check(bool(source), "Joint failure source capture failed");
	const auto baseline = prepareTerrain(map,config,profile,preparationBudget);
	check(bool(baseline), "Joint fault baseline failed");
	check(failedAs(prepareTerrain(map,config,profile,{}),PreparationInputError::MemoryLimit), "Default joint budget did not fail closed");
	ordinal = 0; failAt = SIZE_MAX; recording = true;
	const auto measured = prepareTerrain(map,config,profile,preparationBudget);
	recording = false;
	check(bool(measured), "Joint allocation baseline failed");
	const auto allocations = ordinal;
	for (size_t i=1; i<=allocations; ++i) {
		ordinal = 0; failAt = i; observed = false; recording = true;
		const auto result = prepareTerrain(map,config,profile,preparationBudget);
		recording = false;
		check(observed && ((!result && std::visit([](auto error) { return error == decltype(error)::AllocationFailed; },result.error())) ||
			(result && result->preview.getFingerprint() == baseline->preview.getFingerprint())), "Joint allocation failure returned partial/wrong output");
		check(snapshotMatchesMap(map,*source), "Joint allocation fault changed source");
		selectionCachesUnchanged();
	}
	const auto retry = prepareTerrain(map,config,profile,preparationBudget);
	check(retry && retry->preview.getFingerprint() == baseline->preview.getFingerprint(), "Joint failure affected deterministic retry");
	auto budget = preparationBudget;
	budget.maxBytes = sizeof(PreparedTerrain) + preparationScratchBytes - 1;
	ordinal = 0; failAt = SIZE_MAX; recording = true;
	const auto noMemory = prepareTerrain(map,config,profile,budget);
	recording = false;
	check(failedAs(noMemory,PreparationInputError::MemoryLimit) && ordinal == 0, "Joint infrastructure limit allocated or ran");
	budget = preparationBudget; budget.maxCells = 15;
	check(failedAs(prepareTerrain(map,config,profile,budget),GenerationError::CellLimit), "Joint rectangle limit ignored");
	budget.maxCells = 0;
	check(failedAs(prepareTerrain(map,config,profile,budget),GenerationError::CellLimit), "Zero joint rectangle limit ignored");
	budget = preparationBudget; budget.maxContextCells = 63;
	check(failedAs(prepareTerrain(map,config,profile,budget),PreparationInputError::ContextLimit), "Joint halo limit ignored");
	budget.maxContextCells = 0;
	check(failedAs(prepareTerrain(map,config,profile,budget),PreparationInputError::ContextLimit), "Zero joint halo limit ignored");
	budget = preparationBudget; budget.maxChanges = 15;
	check(failedAs(prepareTerrain(map,config,profile,budget),PreparationInputError::ChangeLimit), "Joint change limit ignored");
	budget.maxChanges = 0;
	check(failedAs(prepareTerrain(map,config,profile,budget),PreparationInputError::ChangeLimit), "Zero joint change limit ignored");
	budget = preparationBudget;
	const auto& memory = baseline->memory;
	budget.maxBytes = memory.infrastructureBytes+memory.semanticBytes+memory.planBytes+memory.snapshotBytes-1;
	check(failedAs(prepareTerrain(map,config,profile,budget),SnapshotError::Limit), "Joint wrapper lost snapshot-stage limit");
	budget = preparationBudget;
	budget.maxChanges = 15;
	std::array<uint8_t,16> mask {};
	mask[15] = 1;
	budget.maxChanges = 1;
	{
		auto borrowedProfile = profile;
		const auto sparse = prepareTerrain(map,config,borrowedProfile,budget,mask);
		check(sparse && sparse->preview.getChanges().size() == 1, "Joint limit rejected valid sparse mask");
		mask[15] = 0;
		borrowedProfile.brushes[0] = "__changed_after_capture__";
		check(sparse->snapshot.input.mask[15] == 1 && sparse->plan.profile.brushes[0] == profile.brushes[0], "Preparation retained borrowed mask/profile storage");
	}
	auto invalidProfile = profile;
	invalidProfile.brushes[0] = std::string(257,'x');
	ordinal = 0; failAt = SIZE_MAX; recording = true;
	const auto invalid = prepareTerrain(map,config,invalidProfile,preparationBudget);
	recording = false;
	check(failedAs(invalid,PreparationInputError::InvalidProfile) && ordinal == 0, "Oversized borrowed profile copied or accepted");
	invalidProfile.brushes[0] = "__missing_preparation_brush__";
	check(failedAs(prepareTerrain(map,config,invalidProfile,preparationBudget),ResolveError::InvalidProfile), "Joint wrapper lost resolver error");
	auto invalidConfig = config;
	invalidConfig.region.x = -1;
	check(failedAs(prepareTerrain(map,invalidConfig,profile,preparationBudget),GenerationError::InvalidRegion), "Joint wrapper lost invalid-region error");
	invalidConfig = config;
	invalidConfig.region.width = invalidConfig.region.height = UINT32_MAX;
	budget = preparationBudget; budget.maxBytes = UINT64_MAX;
	check(failedAs(prepareTerrain(map,invalidConfig,profile,budget),GenerationError::InvalidRegion), "Oversized joint rectangle escaped actual map bounds");
	check(failedAs(prepareTerrain(map,config,profile,preparationBudget,mask),GenerationError::NoWritableCells), "Joint wrapper accepted empty mask");
	std::stop_source cancel;
	cancel.request_stop();
	check(failedAs(prepareTerrain(map,config,profile,preparationBudget,{},cancel.get_token()),PreparationInputError::Cancelled), "Joint pre-Cancel ran");
	bool wrongThread = false;
	std::jthread worker([&] { wrongThread = failedAs(prepareTerrain(map,config,profile,preparationBudget),PreparationInputError::WrongThread); });
	worker.join();
	check(wrongThread && snapshotMatchesMap(map,*source), "Joint worker touched assets/source");
	ordinal = 0; failAt = SIZE_MAX; recording = true;
	const auto clientView = g_version.GetCurrentVersionIDView();
	static_assert(std::is_same_v<decltype(clientView),const std::string_view>);
	const bool currentClient = clientView == "1098-BlackTek";
	recording = false;
	check(currentClient && ordinal == 0, "Client budget preflight copied identifier");
	Map protectedMap;
	protectedMap.initializeEmpty();
	const auto* water = g_brushes.getBrush("clear water")->as<GroundBrush>();
	auto* protectedTile = protectedMap.createTile(104,104,7);
	protectedTile->ground = Item::Create(water->getBorderItems().front().id);
	protectedTile->ground->setUniqueID(7001);
	const GenerationConfig grassConfig {{100,100,7,8,8},1729,1,0,0};
	const auto protectedSource = captureGenerationSnapshot(protectedMap,grassConfig,coreBudget,snapshotBudget);
	check(bool(protectedSource), "Joint protected fixture capture failed");
	check(failedAs(prepareTerrain(protectedMap,grassConfig,profile,preparationBudget),PreviewError::OutOfMask) &&
		snapshotMatchesMap(protectedMap,*protectedSource), "Joint wrapper lost concrete diff refusal or changed protected source");
	selectionCachesUnchanged();
	report << "PREPARATION_FAILURES_PASS\t" << allocations << '\n';
}

void baseGroundVariant(std::ostream& report) {
	Map map;
	map.initializeEmpty();
	const GenerationConfig config {{100,100,7,4,4},1729,1,0,0};
	const auto* grass = g_brushes.getBrush("grass")->as<GroundBrush>();
	check(grass && grass->getBorderItems().size() > 1, "Grass variants missing");
	const auto variant = grass->getBorderItems()[0].id, base = grass->getBorderItems()[1].id;
	const auto oldGroup = g_item_definitions.get(variant).group();
	const auto oldBase = g_item_definitions.get(variant).attribute(ItemAttributeKey::BorderBaseGroundId);
	g_item_definitions.setGroup(variant,ITEM_GROUP_NONE);
	g_item_definitions.setAttribute(variant,ItemAttributeKey::BorderBaseGroundId,base);
	try {
		const auto snapshot = captureGenerationSnapshot(map,config,coreBudget,snapshotBudget);
		check(bool(snapshot), "Base variant capture failed");
		const auto plan = resolve(*snapshot);
		check(std::ranges::find(plan.items,variant) != plan.items.end(), "Fixture did not roll non-ground variant");
		const auto preview = DrawOperations::prepareTerrainPreview(map,*snapshot,plan,previewBudget);
		check(preview && snapshotMatchesMap(map,*snapshot), "Resolver/base-ground preview contract disagrees");
		for (size_t i=0; i<plan.items.size(); ++i) {
			const auto* tile = preview->getBuffer().getTile(100+int(i%4),100+int(i/4),7);
			check(tile && tile->ground && tile->ground->getID() == (plan.items[i] == variant ? base : plan.items[i]), "Base-ground placement differs from shared TerrainPlacement");
		}
	} catch (...) {
		g_item_definitions.setGroup(variant,oldGroup);
		g_item_definitions.setAttribute(variant,ItemAttributeKey::BorderBaseGroundId,oldBase);
		throw;
	}
	g_item_definitions.setGroup(variant,oldGroup);
	g_item_definitions.setAttribute(variant,ItemAttributeKey::BorderBaseGroundId,oldBase);
	report << "PREVIEW_BASE_GROUND_PASS\n";
}

void exactSelectionSize(uint32_t side, std::ostream& report) {
	auto& editor = *observedEditors[side == 100 ? 0 : 1];
	const bool active = g_gui.GetCurrentEditor() == &editor;
	const GenerationRegion region {1000,1000,7,side,side};
	const size_t hole = size_t(side/2)*side + side/2;
	const size_t count = size_t(side)*side;
	editor.selection.start(Selection::INTERNAL);
	editor.selection.clear();
	for (size_t i=0; i<count; ++i) {
		if (i == hole) continue;
		auto* tile = editor.map.createTile(region.x+int(i%side),region.y+int(i/side),region.z);
		check(tile->empty() && !tile->isSelected(), "Selection-mask fixture requires empty unflagged members");
		editor.selection.addInternal(tile);
	}
	editor.selection.finish(Selection::INTERNAL);
	const auto minimum = editor.selection.minPosition(), maximum = editor.selection.maxPosition();
	const auto revision = editor.map.getChangeTracker().getGeneration();
	const auto randomBefore = getRandomGenerator();
	const auto tileCount = editor.map.getTileCount();
	const SelectionMaskBudget budget {count,sizeof(GenerationSelection)+32+count};
	ordinal = 0; failAt = SIZE_MAX; recording = true;
	const auto selected = captureGenerationSelection(editor,budget);
	recording = false;
	check(selected && ordinal == 1 && selected->region == region && selected->selectedCells == count-1 &&
		selected->mask.size() == count && selected->estimatedBytes == budget.maxBytes, "Exact selection size/ownership/budget mismatch");
	for (size_t i=0; i<count; ++i) check(selected->mask[i] == (i == hole ? 0 : 1), "Selection hole filled or member lost");
	ordinal = 0; failAt = 1; observed = false; recording = true;
	const auto valid = validateGenerationSelection(editor,*selected);
	recording = false;
	check(valid && ordinal == 0 && !observed, "Selection revalidation allocated or rejected current membership");
	check(editor.map.getTileCount() == tileCount && editor.map.getChangeTracker().getGeneration() == revision &&
		editor.selection.minPosition() == minimum && editor.selection.maxPosition() == maximum,
		"Selection capture/revalidation mutated map or bounds");
	check(!validateGenerationSelection(*observedEditors[side == 100 ? 1 : 0],*selected), "Selection accepted another editor/map");
	const GenerationConfig config {selected->region,1729,1,0,0};
	const auto prepared = prepareTerrain(editor.map,config,profile,preparationBudget,selected->mask);
	check(prepared && prepared->preview.getChanges().size() == count-1 && snapshotMatchesMap(editor.map,prepared->snapshot),
		"Exact selected empty cells did not prepare without source mutation");
	check(bool(validatePreparedTerrain(editor.map,*prepared,config,profile,selected->mask)), "Live editor exact-mask prepared validation failed");
	if (active) {
		ordinal = 0; failAt = 1; observed = false; recording = true;
		const auto selectedTarget = validateSelectedTerrainTarget(*prepared,*selected,config,profile);
		recording = false;
		check(selectedTarget && *selectedTarget == &editor && ordinal == 0 && !observed, "Exact selection target allocated or rejected active map");
	}
	check(prepared->snapshot.input.mask == selected->mask && prepared->plan.items[hole] == 0, "Prepared mask differs from actual membership");
	for (const auto& position : prepared->preview.getChanges()) {
		const size_t i = size_t(position.y-region.y)*side+size_t(position.x-region.x);
		check(i < count && selected->mask[i] && editor.map.getTile(position)->empty(), "Preview escaped exact selection or wrote source");
	}
	const Position removed(region.x+1,region.y,region.z), added(region.x+int(hole%side),region.y+int(hole/side),region.z);
	auto* removedTile = editor.map.getTile(removed);
	auto* addedTile = editor.map.createTile(added.x,added.y,added.z);
	editor.selection.start(Selection::INTERNAL);
	editor.selection.removeInternal(removedTile);
	editor.selection.addInternal(addedTile);
	editor.selection.finish(Selection::INTERNAL);
	check(editor.selection.minPosition() == minimum && editor.selection.maxPosition() == maximum && editor.selection.size() == count-1,
		"Same-bounds stale selection fixture changed count/bounds");
	const auto stale = validateGenerationSelection(editor,*selected);
	check(!stale && stale.error() == SelectionMaskError::StaleSelection, "Same-bounds membership swap accepted");
	editor.selection.start(Selection::INTERNAL);
	editor.selection.removeInternal(addedTile);
	editor.selection.addInternal(removedTile);
	editor.selection.finish(Selection::INTERNAL);
	check(bool(validateGenerationSelection(editor,*selected)), "Exact restored membership failed revalidation");
	check(!snapshotMatchesMap(editor.map,prepared->snapshot), "Independent source snapshot missed new empty tile in hole");
	check(editor.map.getTileCount() == tileCount+1 && editor.map.getChangeTracker().getGeneration() >= revision &&
		getRandomGenerator() == randomBefore && (g_gui.GetCurrentEditor() == &editor) == active, "Mask API changed source/RNG/current editor");
	const auto fingerprint = prepared->preview.getFingerprint();
	editor.selection.start(Selection::INTERNAL);
	editor.selection.clear();
	auto* marker = editor.map.getTile(markerPosition);
	marker->ground->select();
	TileOperations::updateSelectionState(marker);
	editor.selection.addInternal(marker);
	editor.selection.finish(Selection::INTERNAL);
	check(selected->mask[0] == 1 && selected->mask[hole] == 0 && selected->selectedCells == count-1, "Owned captured mask changed with live selection");
	check(!validateGenerationSelection(editor,*selected), "Cleared membership accepted");
	check(editor.selection.minPosition() == markerPosition && editor.selection.maxPosition() == markerPosition, "Mask fixture marker bounds not restored");
	selectionCachesUnchanged();
	report << "EXACT_SELECTION_SIZE_PASS\t" << side << '\t' << count-1 << '\t' << selected->estimatedBytes << '\t' << fingerprint << '\n';
}

void exactSelectionFailures(std::ostream& report) {
	auto& editor = *observedEditors[0];
	constexpr SelectionMaskBudget budget {250000,1024*1024};
	const auto original = captureGenerationSelection(editor,budget);
	check(original && original->selectedCells == 1 && original->mask == std::vector<uint8_t>{1}, "Marker membership capture failed");
	selectionCachesUnchanged();
	const auto revision = editor.map.getChangeTracker().getGeneration();
	const auto randomBefore = getRandomGenerator();
	auto rejected = [&](SelectionMaskBudget limit, SelectionMaskError error) {
		ordinal = 0; failAt = SIZE_MAX; recording = true;
		const auto result = captureGenerationSelection(editor,limit);
		recording = false;
		check(!result && result.error() == error && ordinal == 0, "Selection limit allocated or wrong error");
	};
	rejected({},SelectionMaskError::CellLimit);
	rejected({1,sizeof(GenerationSelection)+32},SelectionMaskError::MemoryLimit);
	ordinal = 0; failAt = 1; observed = false; recording = true;
	const auto fault = captureGenerationSelection(editor,budget);
	recording = false;
	check(!fault && fault.error() == SelectionMaskError::AllocationFailed && observed && ordinal == 1,
		"Mask allocation fault escaped or returned partial result");
	check(bool(validateGenerationSelection(editor,*original)), "Mask allocation fault changed membership");
	std::stop_source stop;
	stop.request_stop();
	const auto cancel = captureGenerationSelection(editor,budget,stop.get_token());
	const auto validateCancel = validateGenerationSelection(editor,*original,stop.get_token());
	check(!cancel && cancel.error() == SelectionMaskError::Cancelled && !validateCancel && validateCancel.error() == SelectionMaskError::Cancelled,
		"Selection pre-cancel ran");
	bool wrongCapture = false, wrongValidate = false;
	std::jthread worker([&] {
		const auto captured = captureGenerationSelection(editor,budget);
		const auto validated = validateGenerationSelection(editor,*original);
		wrongCapture = !captured && captured.error() == SelectionMaskError::WrongThread;
		wrongValidate = !validated && validated.error() == SelectionMaskError::WrongThread;
	});
	worker.join();
	check(wrongCapture && wrongValidate, "Selection worker accessed live editor");
	editor.selection.start(Selection::INTERNAL);
	const auto busy = captureGenerationSelection(editor,budget);
	const auto validateBusy = validateGenerationSelection(editor,*original);
	editor.selection.finish(Selection::INTERNAL);
	check(!busy && busy.error() == SelectionMaskError::Busy && !validateBusy && validateBusy.error() == SelectionMaskError::Busy,
		"Selection pending session was read");
	auto malformed = *original;
	malformed.mask[0] = 2;
	check(!validateGenerationSelection(editor,malformed), "Nonbinary stored mask accepted");
	malformed = *original; malformed.mask.clear();
	check(!validateGenerationSelection(editor,malformed), "Short stored mask accepted");
	malformed = *original; malformed.selectedCells = 0;
	check(!validateGenerationSelection(editor,malformed), "Wrong stored membership count accepted");
	malformed = *original; malformed.region.width = malformed.region.height = UINT32_MAX;
	check(!validateGenerationSelection(editor,malformed), "Oversized stored mask geometry accepted");
	malformed = *original; malformed.region.x = -1;
	check(!validateGenerationSelection(editor,malformed), "Negative stored geometry accepted");
	malformed = *original; malformed.region.x -= 1; malformed.region.width = 2; malformed.mask = {0,1};
	check(!validateGenerationSelection(editor,malformed), "Padded stored bounds accepted");
	auto* marker = editor.map.getTile(markerPosition);
	auto* markerLocation = marker->getLocation();
	markerLocation->setPosition(Position(-1,markerPosition.y,markerPosition.z));
	const auto negative = captureGenerationSelection(editor,budget);
	markerLocation->setPosition(markerPosition);
	check(!negative && negative.error() == SelectionMaskError::InvalidSelection, "Negative live member accepted");
	markerLocation->setPosition(Position(editor.map.getWidth(),markerPosition.y,markerPosition.z));
	const auto outside = captureGenerationSelection(editor,budget);
	markerLocation->setPosition(markerPosition);
	check(!outside && outside.error() == SelectionMaskError::InvalidSelection, "Outside-header member accepted");
	marker->ground->deselect();
	TileOperations::updateSelectionState(marker);
	check(bool(validateGenerationSelection(editor,*original)), "Item selection flags changed exact membership mask");
	marker->ground->select();
	TileOperations::updateSelectionState(marker);
	// Replace a fixture tile without retaining its old pointer in Selection.
	// Membership is positional; source revision validation is a separate gate.
	auto replacement = marker->deepCopy();
	editor.selection.start(Selection::INTERNAL);
	editor.selection.removeInternal(marker);
	editor.selection.finish(Selection::INTERNAL);
	auto displaced = editor.map.swapTile(markerPosition,std::move(replacement));
	auto* installed = editor.map.getTile(markerPosition);
	editor.selection.start(Selection::INTERNAL);
	editor.selection.addInternal(installed);
	editor.selection.finish(Selection::INTERNAL);
	check(installed != marker && validateGenerationSelection(editor,*original), "Same membership with replaced live tile rejected");
	editor.selection.start(Selection::INTERNAL);
	editor.selection.removeInternal(installed);
	editor.selection.finish(Selection::INTERNAL);
	auto retired = editor.map.swapTile(markerPosition,std::move(displaced));
	editor.selection.start(Selection::INTERNAL);
	editor.selection.addInternal(marker);
	editor.selection.finish(Selection::INTERNAL);
	auto detached = marker->deepCopy();
	editor.selection.start(Selection::INTERNAL);
	editor.selection.removeInternal(marker);
	editor.selection.addInternal(detached.get());
	editor.selection.finish(Selection::INTERNAL);
	const auto foreign = captureGenerationSelection(editor,budget);
	check(!foreign && foreign.error() == SelectionMaskError::InvalidSelection && !validateGenerationSelection(editor,*original),
		"Detached same-position member accepted");
	editor.selection.start(Selection::INTERNAL);
	editor.selection.removeInternal(detached.get());
	editor.selection.addInternal(observedEditors[1]->map.getTile(markerPosition));
	editor.selection.finish(Selection::INTERNAL);
	const auto otherMap = captureGenerationSelection(editor,budget);
	check(!otherMap && otherMap.error() == SelectionMaskError::InvalidSelection, "Other-map member accepted");
	editor.selection.start(Selection::INTERNAL);
	editor.selection.removeInternal(observedEditors[1]->map.getTile(markerPosition));
	editor.selection.addInternal(marker);
	auto* upper = editor.map.createTile(markerPosition.x,markerPosition.y,6);
	editor.selection.addInternal(upper);
	editor.selection.finish(Selection::INTERNAL);
	const auto floors = captureGenerationSelection(editor,budget);
	check(!floors && floors.error() == SelectionMaskError::MultipleFloors, "Multi-floor selection silently projected");
	editor.selection.start(Selection::INTERNAL);
	editor.selection.removeInternal(upper);
	editor.selection.removeInternal(marker);
	editor.selection.finish(Selection::INTERNAL);
	const auto empty = captureGenerationSelection(editor,budget);
	check(!empty && empty.error() == SelectionMaskError::Empty, "Empty selection expanded to rectangle");
	editor.selection.start(Selection::INTERNAL);
	editor.selection.addInternal(marker);
	editor.selection.finish(Selection::INTERNAL);
	check(validateGenerationSelection(editor,*original) && editor.map.getChangeTracker().getGeneration() >= revision &&
		getRandomGenerator() == randomBefore, "Selection failure changed source/membership/RNG");
	check(editor.selection.minPosition() == markerPosition && editor.selection.maxPosition() == markerPosition, "Failure fixture marker bounds not restored");
	selectionCachesUnchanged();
	report << "EXACT_SELECTION_FAILURES_PASS\t1\n";
}

void preparedValidationFailures(std::ostream& report) {
	Map map;
	map.initializeEmpty();
	const auto* grass = g_brushes.getBrush("grass")->as<GroundBrush>();
	auto* halo = map.createTile(198,200,7);
	halo->ground = Item::Create(grass->getBorderItems().front().id);
	halo->ground->setUniqueID(7001);
	map.createTile(200,200,7)->statflags = TILESTATE_SELECTED;
	auto* location = map.createTileL(199,201,7);
	location->increaseTownCount();
	const GenerationConfig config {{200,200,7,8,8},1729,1,0,0};
	std::vector<uint8_t> mask(64,1);
	auto prepared = prepareTerrain(map,config,profile,preparationBudget,mask);
	check(bool(prepared), "Prepared validation fixture failed");
	const auto randomBefore = getRandomGenerator();
	const auto countBefore = map.getTileCount(), revisionBefore = map.getChangeTracker().getGeneration();
	size_t cases = 0;
	auto rejected = [&](const auto& result, PreparedValidationError error) {
		check(!result && result.error() == error, "Prepared validation accepted stale/tampered state or wrong error");
		++cases;
	};
	auto valid = [&] { return validatePreparedTerrain(map,*prepared,config,profile,mask); };
	check(valid() && validatePreparedTerrain(map,*prepared,config,profile), "Empty and full-one request masks disagreed");
	Map other;
	other.initializeEmpty();
	rejected(validatePreparedTerrain(other,*prepared,config,profile,mask),PreparedValidationError::StaleMap);
	for (int field=0; field<9; ++field) {
		auto changed = config;
		switch (field) {
			case 0: ++changed.seed; break;
			case 1: ++changed.grassWeight; break;
			case 2: ++changed.dirtWeight; break;
			case 3: ++changed.waterWeight; break;
			case 4: ++changed.region.x; break;
			case 5: ++changed.region.y; break;
			case 6: ++changed.region.z; break;
			case 7: ++changed.region.width; break;
			case 8: ++changed.region.height; break;
		}
		rejected(validatePreparedTerrain(map,*prepared,changed,profile,mask),PreparedValidationError::StaleRequest);
	}
	auto changedProfile = profile;
	changedProfile.brushes[1] = "grass";
	rejected(validatePreparedTerrain(map,*prepared,config,changedProfile,mask),PreparedValidationError::StaleRequest);
	mask[1] = 0;
	rejected(valid(),PreparedValidationError::StaleRequest);
	mask[1] = 2;
	rejected(valid(),PreparedValidationError::StaleRequest);
	mask[1] = 1;
	rejected(validatePreparedTerrain(map,*prepared,config,profile,std::span(mask).first(63)),PreparedValidationError::StaleRequest);
	std::stop_source stop;
	stop.request_stop();
	rejected(validatePreparedTerrain(map,*prepared,config,profile,mask,stop.get_token()),PreparedValidationError::Cancelled);
	bool workerRefused = false;
	std::jthread worker([&] {
		const auto result = valid();
		workerRefused = !result && result.error() == PreparedValidationError::WrongThread;
	});
	worker.join();
	check(workerRefused, "Prepared validation read GUI state from worker");
	++cases;
	// Deliberate changes to public owned fields cannot rebind an old private preview stamp.
	const auto originalId = prepared->plan.items[0];
	prepared->plan.items[0] = 0;
	rejected(valid(),PreparedValidationError::InvalidPrepared);
	prepared->plan.items[0] = originalId;
	prepared->plan.profile.brushes[1] = "grass";
	rejected(validatePreparedTerrain(map,*prepared,config,changedProfile,mask),PreparedValidationError::InvalidPrepared);
	prepared->plan.profile = profile;
	++prepared->plan.semanticFingerprint;
	rejected(valid(),PreparedValidationError::InvalidPrepared);
	--prepared->plan.semanticFingerprint;
	prepared->snapshot.input.mask[1] = mask[1] = 0;
	rejected(valid(),PreparedValidationError::InvalidPrepared);
	prepared->snapshot.input.mask[1] = mask[1] = 1;
	prepared->snapshot.input.protection[1] = 1;
	rejected(valid(),PreparedValidationError::InvalidPrepared);
	prepared->snapshot.input.protection[1] = 0;
	++prepared->snapshot.input.extent.width;
	rejected(valid(),PreparedValidationError::InvalidPrepared);
	--prepared->snapshot.input.extent.width;
	const auto savedMask = prepared->snapshot.input.mask;
	prepared->snapshot.input.mask.pop_back();
	rejected(valid(),PreparedValidationError::InvalidPrepared);
	prepared->snapshot.input.mask = savedMask;
	const auto& position = prepared->preview.getChanges().front();
	auto* generatedGround = prepared->preview.getBuffer().getTile(position)->ground.get();
	const bool wasSelected = generatedGround->isSelected();
	generatedGround->select();
	rejected(valid(),PreparedValidationError::InvalidPrepared);
	if (!wasSelected) generatedGround->deselect();
	auto differentConfig = config;
	++differentConfig.seed;
	auto different = prepareTerrain(map,differentConfig,profile,preparationBudget,mask);
	check(bool(different), "Mixed preview fixture failed");
	std::swap(prepared->preview,different->preview);
	rejected(valid(),PreparedValidationError::InvalidPrepared);
	std::swap(prepared->preview,different->preview);
	// Owned source-copy changes are checked independently against the live source.
	const size_t haloIndex = size_t(halo->getY()-prepared->snapshot.context.y)*prepared->snapshot.context.width+
		size_t(halo->getX()-prepared->snapshot.context.x);
	prepared->snapshot.cells[haloIndex].original->ground->setUniqueID(7002);
	rejected(valid(),PreparedValidationError::StaleMap);
	prepared->snapshot.cells[haloIndex].original->ground->setUniqueID(7001);
	halo->ground->setActionID(77);
	check(map.getChangeTracker().getGeneration() == revisionBefore, "Direct source edit unexpectedly changed revision fixture");
	rejected(valid(),PreparedValidationError::StaleMap);
	halo->ground->setActionID(0);
	// Attribute absence differs from an explicitly stored zero; restore exactly.
	halo->ground->eraseAttribute(Item::ATTR_AID);
	location->increaseTownCount();
	rejected(valid(),PreparedValidationError::StaleMap);
	location->decreaseTownCount();
	check(valid() && map.getTileCount() == countBefore && map.getChangeTracker().getGeneration() == revisionBefore,
		"Validation cases changed source instead of only observing it");
	map.getChangeTracker().markAllDirty();
	rejected(valid(),PreparedValidationError::StaleMap);
	check(getRandomGenerator() == randomBefore, "Prepared validation advanced editor RNG");
	selectionCachesUnchanged();
	report << "PREPARED_VALIDATION_FAILURES_PASS\t" << cases << '\n';
}

// A registry-owned fixture exposes only its own protected data for mutations.
// Production classes have no test setters and no borrowed private views.
class AssetProbeBrush final : public GroundBrush {
public:
	explicit AssetProbeBrush(uint16_t item) {
		name = "__g1_asset_catalog_probe__";
		total_chance = 1;
		border_items.push_back({1,item});
		owned_optional_border = std::make_unique<AutoBorder>(0);
		optional_border = owned_optional_border.get();
		auto block = std::make_unique<BorderBlock>();
		block->outer = true; block->super = false; block->to = 0;
		block->owned_autoborder = std::make_unique<AutoBorder>(0);
		block->autoborder = block->owned_autoborder.get();
		auto specific = std::make_unique<SpecificCaseBlock>();
		specific->items_to_match.push_back(item);
		block->specific_cases.push_back(std::move(specific));
		borders.push_back(std::move(block));
	}
	int32_t& order() { return z_order; }
	int& chance() { return total_chance; }
	std::vector<ItemChanceBlock>& variants() { return border_items; }
	std::vector<uint32_t>& relations() { return friends; }
	bool& hates() { return hate_friends; }
	bool& randomizes() { return randomize; }
	AutoBorder*& optional() { return optional_border; }
};

void generationAssetMutations(std::ostream& report) {
	const auto* grass = g_brushes.getBrush("grass")->as<GroundBrush>();
	const auto id = grass->getBorderItems().front().id;
	// Retain this mutable reference BEFORE preparing; no setter/revision call
	// occurs when the association is subsequently changed through this reference.
	auto& retainedEditorData = g_item_definitions.mutableEditorData(id);
	auto owned = std::make_unique<AssetProbeBrush>(id);
	auto* fixture = owned.get();
	g_brushes.addBrush(std::move(owned));
	Map map;
	map.initializeEmpty();
	const GenerationConfig config {{200,200,7,8,8},1729,1,0,0};
	const auto prepared = prepareTerrain(map,config,profile,preparationBudget);
	check(bool(prepared), "Asset mutation baseline failed");
	const auto epoch = g_version.GetAssetRevision();
	const auto randomBefore = getRandomGenerator();
	ordinal = 0; failAt = 1; observed = false; recording = true;
	const auto begin = std::chrono::steady_clock::now();
	const auto baseline = fingerprintGenerationAssets();
	const auto end = std::chrono::steady_clock::now();
	recording = false;
	check(baseline && ordinal == 0 && !observed, "Catalog fingerprint allocated");
	size_t cases = 0;
	auto changed = [&] {
		const auto current = fingerprintGenerationAssets();
		check(!current || *current != *baseline, "Covered asset mutation kept catalog fingerprint");
		const auto result = validatePreparedTerrain(map,*prepared,config,profile);
		check(!result && result.error() == PreparedValidationError::StaleAssets && g_version.GetAssetRevision() == epoch,
			"Independent asset mutation was not refused before source/preview access");
		++cases;
	};
	auto restored = [&] {
		check(fingerprintGenerationAssets() == baseline && validatePreparedTerrain(map,*prepared,config,profile),
			"Exact catalog restoration did not restore validation");
	};
	auto scalar = [&](auto& field, auto replacement) {
		const auto previous = field;
		field = replacement;
		changed();
		field = previous;
		restored();
	};
	const auto definition = g_item_definitions.get(id);
	const auto oldGroup = definition.group();
	g_item_definitions.setGroup(id,ITEM_GROUP_NONE); changed();
	g_item_definitions.setGroup(id,oldGroup); restored();
	const bool blocking = definition.hasFlag(ItemFlag::Unpassable);
	g_item_definitions.setFlag(id,ItemFlag::Unpassable,!blocking); changed();
	g_item_definitions.setFlag(id,ItemFlag::Unpassable,blocking); restored();
	const auto oldType = definition.type();
	g_item_definitions.setType(id,ITEM_TYPE_CONTAINER); changed();
	g_item_definitions.setType(id,oldType); restored();
	const auto base = definition.attribute(ItemAttributeKey::BorderBaseGroundId);
	g_item_definitions.setAttribute(id,ItemAttributeKey::BorderBaseGroundId,base+1); changed();
	g_item_definitions.setAttribute(id,ItemAttributeKey::BorderBaseGroundId,base); restored();
	const std::string oldName(definition.name());
	g_item_definitions.setName(id,oldName+"_changed"); changed();
	g_item_definitions.setName(id,oldName); restored();
	scalar(retainedEditorData.brush,static_cast<Brush*>(nullptr));
	scalar(fixture->order(),fixture->order()+1);
	scalar(fixture->chance(),fixture->chance()+1);
	scalar(fixture->variants()[0].id,uint16_t(id+1));
	scalar(fixture->variants()[0].chance,fixture->variants()[0].chance+1);
	fixture->relations().push_back(Brush::WILDCARD_ID); changed();
	fixture->relations().pop_back(); restored();
	scalar(fixture->hates(),!fixture->hates());
	scalar(fixture->randomizes(),!fixture->randomizes());
	auto& block = *fixture->getBorders().front();
	scalar(block.outer,!block.outer);
	scalar(block.super,!block.super);
	scalar(block.to,block.to+1);
	auto& specific = *block.specific_cases.front();
	scalar(specific.match_group,specific.match_group+1);
	scalar(specific.items_to_match.front(),uint16_t(specific.items_to_match.front()+1));
	scalar(specific.to_replace_id,uint16_t(specific.to_replace_id+1));
	scalar(specific.with_id,uint16_t(specific.with_id+1));
	scalar(specific.delete_all,!specific.delete_all);
	scalar(specific.keepBorder,!specific.keepBorder);
	scalar(block.owned_autoborder->tiles[1],block.owned_autoborder->tiles[1]+1);
	scalar(block.owned_autoborder->group,uint16_t(block.owned_autoborder->group+1));
	scalar(block.owned_autoborder->ground,!block.owned_autoborder->ground);
	scalar(fixture->optional()->tiles[2],fixture->optional()->tiles[2]+1);
	AutoBorder unowned(0);
	scalar(fixture->optional(),&unowned);
	// The pointer token may outlive its target. Catalog checks must refuse
	// before Item::getGroundBrush or a border calculation could follow it.
	auto* previousAssociation = retainedEditorData.brush;
	auto transientBrush = std::make_unique<AssetProbeBrush>(id);
	retainedEditorData.brush = transientBrush.get();
	transientBrush.reset();
	changed();
	retainedEditorData.brush = previousAssociation; restored();
	auto* previousOptional = fixture->optional();
	auto transientBorder = std::make_unique<AutoBorder>(0);
	fixture->optional() = transientBorder.get();
	transientBorder.reset();
	changed();
	fixture->optional() = previousOptional; restored();
	auto displaced = std::move(block.specific_cases.front());
	changed();
	block.specific_cases.front() = std::move(displaced); restored();
	check(!g_brushes.getBorders().empty(), "Global border fixture absent");
	auto& globalBorder = *g_brushes.getBorders().begin()->second;
	scalar(globalBorder.tiles[1],globalBorder.tiles[1]+1);
	scalar(globalBorder.group,uint16_t(globalBorder.group+1));
	scalar(GroundBrush::border_types[255],GroundBrush::border_types[255]^1U);
	auto* sprite = g_gui.gfx.getGameSprite(int(definition.clientId()));
	check(sprite != nullptr, "Loaded sprite metadata missing");
	scalar(sprite->minimap_color,uint16_t(sprite->minimap_color^1U));
	scalar(sprite->has_light,!sprite->has_light);
	scalar(sprite->light.intensity,uint8_t(sprite->light.intensity+1));
	scalar(sprite->light.color,uint8_t(sprite->light.color+1));
	scalar(sprite->draw_height,uint16_t(sprite->draw_height+1));
	check(!sprite->sprite_ids.empty(), "Sprite source IDs missing");
	scalar(sprite->sprite_ids.front(),sprite->sprite_ids.front()+1);
	// Palette visibility and derived geometry/cache state are deliberately not assets.
	fixture->flagAsVisible();
	sprite->invalidateMetricCaches();
	(void)sprite->getDrawOffset();
	restored();
	std::stop_source stop;
	stop.request_stop();
	const auto cancelled = fingerprintGenerationAssets(stop.get_token());
	check(!cancelled && cancelled.error() == AssetFingerprintError::Cancelled, "Asset fingerprint ignored pre-cancel");
	bool wrongThread = false;
	std::jthread worker([&] {
		const auto result = fingerprintGenerationAssets();
		wrongThread = !result && result.error() == AssetFingerprintError::WrongThread;
	});
	worker.join();
	check(wrongThread && map.getTileCount() == 0 && snapshotMatchesMap(map,prepared->snapshot) && getRandomGenerator() == randomBefore,
		"Catalog checks changed source/RNG or read GUI state from worker");
	selectionCachesUnchanged();
	report << "GENERATION_ASSET_MUTATIONS_PASS\t" << cases << "\t0\t" <<
		std::chrono::duration<double,std::milli>(end-begin).count() << '\n';
}

template<typename Error>
bool targetFailedAs(const std::expected<Editor*,TargetValidationError>& result, Error error) {
	const auto* actual = result ? nullptr : std::get_if<Error>(&result.error());
	return actual && *actual == error;
}

void terrainTargetSize(uint32_t side, std::ostream& report) {
	auto* editor = g_gui.GetCurrentEditor();
	check(editor == observedEditors[1], "Target size fixture needs current observed editor");
	const GenerationConfig config {{400,100,7,side,side},1729,1,0,0};
	const auto prepared = prepareTerrain(editor->map,config,profile,preparationBudget);
	check(bool(prepared), "Real editor target preparation failed");
	const auto randomBefore = getRandomGenerator();
	const auto revision = editor->map.getChangeTracker().getGeneration();
	const auto count = editor->map.getTileCount();
	const auto history = editor->actionQueue->getCurrentIndex(), historySize = editor->actionQueue->getSize();
	ordinal = 0; failAt = 1; observed = false; recording = true;
	const auto target = validateTerrainTarget(*prepared,config,profile);
	recording = false;
	check(target && *target == editor && ordinal == 0 && !observed, "Current target guard allocated or rejected correct editor");
	check(snapshotMatchesMap(editor->map,prepared->snapshot) && revision == editor->map.getChangeTracker().getGeneration() &&
		count == editor->map.getTileCount() && history == editor->actionQueue->getCurrentIndex() && historySize == editor->actionQueue->getSize() &&
		getRandomGenerator() == randomBefore, "Target validation changed source/history/RNG");
	selectionCachesUnchanged();
	report << "TERRAIN_TARGET_SIZE_PASS\t" << side << "\t0\t" << prepared->preview.getFingerprint() << '\n';
}

void terrainTargetLifecycle(std::ostream& report) {
	check(g_gui.GetCurrentEditor() == nullptr, "Target lifecycle requires closed observer tabs");
	check(g_gui.NewMap(), "Target lifecycle map creation failed");
	auto* editor = g_gui.GetCurrentEditor();
	const auto mapVersion = editor->map.getVersion();
	const GenerationConfig config {{400,100,7,4,4},1729,1,0,0};
	auto prepared = prepareTerrain(editor->map,config,profile,preparationBudget);
	check(prepared && validateTerrainTarget(*prepared,config,profile), "Fresh lifecycle target failed");
	size_t cases = 0;
	auto rejected = [&](const auto& result, auto error) {
		check(targetFailedAs(result,error), "Target guard accepted invalid state or lost error domain");
		++cases;
	};
	auto current = [&] { return validateTerrainTarget(*prepared,config,profile); };
	auto* originalWindow = g_gui.GetCurrentMapTab()->GetWindow();
	g_editors.NewMapView();
	check(g_gui.GetCurrentEditor() == editor && current(), "Second view of same unchanged editor rejected");
	auto* sharedWindow = g_gui.GetCurrentMapTab()->GetWindow();
	g_gui.tabbook->DeleteTab(g_gui.tabbook->GetTabIndex(originalWindow));
	originalWindow = nullptr;
	check(g_gui.GetCurrentEditor() == editor && current(), "Closing one shared view invalidated still-owned editor");
	check(g_gui.NewMap(), "Other target map creation failed");
	rejected(current(),TerrainTargetError::WrongMap);
	g_gui.tabbook->SetFocusedTab(g_gui.tabbook->GetTabIndex(sharedWindow));
	check(current() && g_gui.GetCurrentEditor() == editor, "Returning to unchanged source editor rejected");
	editor->selection.start(Selection::INTERNAL);
	rejected(current(),TerrainTargetError::BusySelection);
	rejected(validateSelectedTerrainTarget(*prepared,{},config,profile),TerrainTargetError::BusySelection);
	editor->selection.finish(Selection::INTERNAL);
	std::stop_source stop;
	stop.request_stop();
	rejected(validateTerrainTarget(*prepared,config,profile,{},stop.get_token()),TerrainTargetError::Cancelled);
	rejected(validateSelectedTerrainTarget(*prepared,{},config,profile,stop.get_token()),TerrainTargetError::Cancelled);
	bool wrongRectangle = false, wrongSelection = false;
	std::jthread worker([&] {
		wrongRectangle = targetFailedAs(current(),TerrainTargetError::WrongThread);
		wrongSelection = targetFailedAs(validateSelectedTerrainTarget(*prepared,{},config,profile),TerrainTargetError::WrongThread);
	});
	worker.join();
	check(wrongRectangle && wrongSelection, "Target guard accessed UI from worker");
	cases += 2;
	auto changedConfig = config;
	++changedConfig.seed;
	rejected(validateTerrainTarget(*prepared,changedConfig,profile),PreparedValidationError::StaleRequest);
	editor->map.getChangeTracker().markAllDirty();
	rejected(current(),PreparedValidationError::StaleMap);
	prepared = prepareTerrain(editor->map,config,profile,preparationBudget);
	check(bool(prepared), "Lifecycle preparation after revision failed");
	const std::array<Position,5> positions {{{400,100,7},{403,100,7},{400,103,7},{403,103,7},{401,101,7}}};
	editor->selection.start(Selection::INTERNAL);
	for (const auto& position : positions) editor->selection.addInternal(editor->map.createTile(position.x,position.y,position.z));
	editor->selection.finish(Selection::INTERNAL);
	const auto selected = captureGenerationSelection(*editor,{16,1024});
	check(bool(selected), "Lifecycle exact selection capture failed");
	rejected(validateSelectedTerrainTarget(*prepared,*selected,config,profile),PreparedValidationError::StaleRequest);
	auto selectedPrepared = prepareTerrain(editor->map,config,profile,preparationBudget,selected->mask);
	check(bool(selectedPrepared), "Lifecycle exact selection preparation failed");
	ordinal = 0; failAt = 1; observed = false; recording = true;
	const auto selectedTarget = validateSelectedTerrainTarget(*selectedPrepared,*selected,config,profile);
	recording = false;
	check(selectedTarget && *selectedTarget == editor && ordinal == 0 && !observed, "Selected target allocated or rejected membership");
	changedConfig = config; ++changedConfig.region.x;
	rejected(validateSelectedTerrainTarget(*selectedPrepared,*selected,changedConfig,profile),SelectionMaskError::StaleSelection);
	auto malformed = *selected;
	++malformed.mapGeneration;
	rejected(validateSelectedTerrainTarget(*selectedPrepared,malformed,config,profile),SelectionMaskError::StaleSelection);
	auto* removed = editor->map.getTile(positions.back());
	auto* added = editor->map.createTile(402,102,7);
	editor->selection.start(Selection::INTERNAL);
	editor->selection.removeInternal(removed); editor->selection.addInternal(added);
	editor->selection.finish(Selection::INTERNAL);
	rejected(validateSelectedTerrainTarget(*selectedPrepared,*selected,config,profile),SelectionMaskError::StaleSelection);
	editor->selection.start(Selection::INTERNAL);
	editor->selection.removeInternal(added); editor->selection.addInternal(removed);
	editor->selection.finish(Selection::INTERNAL);
	rejected(validateSelectedTerrainTarget(*selectedPrepared,*selected,config,profile),PreparedValidationError::StaleMap);
	// Last-view closure destroys the source; retained inputs own their data.
	const auto oldGeneration = prepared->snapshot.input.mapGeneration;
	editor->actionQueue->clear(); editor->map.clearChanges();
	editor = nullptr; sharedWindow = nullptr;
	check(g_gui.CloseAllEditors() && !g_gui.GetCurrentEditor(), "Lifecycle tabs did not close");
	rejected(current(),TerrainTargetError::NoEditor);
	rejected(validateSelectedTerrainTarget(*selectedPrepared,*selected,config,profile),TerrainTargetError::NoEditor);
	check(g_gui.NewMap(), "Replacement target map creation failed");
	check(g_gui.GetCurrentEditor()->map.getGeneration() != oldGeneration, "Replacement map reused source generation");
	rejected(current(),TerrainTargetError::WrongMap);
	rejected(validateSelectedTerrainTarget(*selectedPrepared,*selected,config,profile),TerrainTargetError::WrongMap);
	check(g_gui.CloseAllEditors(), "Replacement map did not close");
	// Inert client object: no connect/resolve/socket/service start is called.
	auto liveOwner = std::make_unique<Editor>(g_gui.copybuffer,mapVersion,std::make_unique<LiveClient>());
	liveOwner->map.initializeEmpty();
	auto* liveEditor = liveOwner.get();
	new MapTab(g_gui.tabbook,liveOwner.get());
	liveOwner.release(); // MapTab's existing InternalReference owns this editor.
	const auto livePrepared = prepareTerrain(liveEditor->map,config,profile,preparationBudget);
	check(livePrepared && liveEditor->live_manager.IsClient(), "Inert live-client fixture failed");
	rejected(validateTerrainTarget(*livePrepared,config,profile),TerrainTargetError::LiveSession);
	rejected(validateSelectedTerrainTarget(*livePrepared,{},config,profile),TerrainTargetError::LiveSession);
	liveEditor->actionQueue->clear(); liveEditor->map.clearChanges();
	liveEditor = nullptr;
	check(g_gui.CloseAllEditors(), "Inert client fixture did not close");
	check(g_gui.NewMap(), "Inert host fixture creation failed");
	auto* host = g_gui.GetCurrentEditor();
	const auto hostPrepared = prepareTerrain(host->map,config,profile,preparationBudget);
	check(bool(hostPrepared), "Inert host preparation failed");
	// StartServer creates an unbound object/queue; no bind, accept or network
	// connection start follows. This tests IsServer rather than an active socket.
	host->live_manager.StartServer();
	check(host->live_manager.IsServer(), "Inert server flag missing");
	rejected(validateTerrainTarget(*hostPrepared,config,profile),TerrainTargetError::LiveSession);
	rejected(validateSelectedTerrainTarget(*hostPrepared,{},config,profile),TerrainTargetError::LiveSession);
	host->live_manager.CloseServer();
	check(bool(validateTerrainTarget(*hostPrepared,config,profile)), "Returning inert host to offline rejected valid target");
	host->actionQueue->clear(); host->map.clearChanges();
	host = nullptr;
	check(g_gui.CloseAllEditors(), "Inert host fixture did not close");
	report << "TERRAIN_TARGET_LIFECYCLE_PASS\t" << cases << "\t2\t0\n";
}

constexpr TerrainCommitBudget commitBudget {250000, 768ULL * 1024 * 1024};

struct CommitSettings {
	int count = g_settings.getInteger(Config::UNDO_SIZE), bytes = g_settings.getInteger(Config::UNDO_MEM_SIZE),
		group = g_settings.getInteger(Config::GROUP_ACTIONS);
	CommitSettings() {
		g_settings.setInteger(Config::UNDO_SIZE,1000);
		g_settings.setInteger(Config::UNDO_MEM_SIZE,512);
		g_settings.setInteger(Config::GROUP_ACTIONS,1);
	}
	~CommitSettings() {
		recording = false;
		g_settings.setInteger(Config::UNDO_SIZE,count);
		g_settings.setInteger(Config::UNDO_MEM_SIZE,bytes);
		g_settings.setInteger(Config::GROUP_ACTIONS,group);
	}
};

bool commitSnapshotEqual(const Map& map, const GenerationSnapshot& snapshot) {
	const auto& c = snapshot.context;
	for (uint32_t y=0;y<c.height;++y) for (uint32_t x=0;x<c.width;++x) {
		const auto& cell = snapshot.cells[size_t(y)*c.width+x];
		const auto* location = map.getTileL(c.x+int(x),c.y+int(y),c.z);
		if (!snapshotTilesEqual(location ? location->get() : nullptr,cell.original.get()) ||
			cell.spawnCount != (location ? location->getSpawnCount() : 0) ||
			cell.waypointCount != (location ? location->getWaypointCount() : 0) ||
			cell.townCount != (location ? location->getTownCount() : 0)) return false;
		const auto* exits = location ? location->getHouseExits() : nullptr;
		if ((!exits && !cell.houseExits.empty()) || (exits && *exits != cell.houseExits)) return false;
	}
	return true;
}

void checkCommitted(const Editor& editor, const PreparedTerrain& prepared) {
	for (const auto& position : prepared.preview.getChanges()) {
		check(snapshotTilesEqual(editor.map.getTile(position),prepared.preview.getBuffer().getTile(position)),
			"Committed/Redone terrain differs from concrete preview");
	}
	for (const auto* member : editor.selection.getTiles()) {
		check(member && editor.map.getTile(member->getPosition()) == member,"Selection retains displaced terrain pointer");
	}
}

void commitManualDraw(Editor& editor, const Position& position, uint16_t id) {
	auto tile = editor.map.getTile(position) ? editor.map.getTile(position)->deepCopy() :
		std::make_unique<Tile>(position.x,position.y,position.z);
	tile->setGround(Item::Create(id));
	auto action = editor.actionQueue->createAction(ACTION_DRAW);
	action->addChange(std::make_unique<Change>(std::move(tile),position));
	editor.actionQueue->addAction(std::move(action));
}

void tileReservationFailures(std::ostream& report) {
	BaseMap map;
	auto* kept = map.createTile(2,2,7);
	auto* metadata = map.createTileL(3,3,7);
	metadata->increaseWaypointCount();
	const auto originalCells = map.getGrid().cellCount();
	const auto originalMask = map.getGrid().getCell(0)->floor_chunk_mask;
	const auto revision = map.getChangeTracker().getGeneration();
	const std::array<Position,8> positions {{{2,2,7},{4,4,7},{63,63,7},{64,64,7},
		{128,128,6},{129,129,7},{64,64,7},{4,4,8}}};
	ordinal=0; failAt=SIZE_MAX; recording=true;
	{ BaseMap::TileReservation reservation(map,positions); }
	recording=false;
	const auto allocations=ordinal;
	for (size_t fault=1;fault<=allocations;++fault) {
		ordinal=0;failAt=fault;observed=false;recording=true;
		try { BaseMap::TileReservation reservation(map,positions); }
		catch (const std::bad_alloc&) {}
		recording=false;
		check(map.getGrid().cellCount()==originalCells && map.getGrid().getCell(0)->floor_chunk_mask==originalMask &&
			map.getTile(2,2,7)==kept && map.getTileL(3,3,7)==metadata && metadata->getWaypointCount()==1 &&
			map.getTileCount()==1 && map.getChangeTracker().getGeneration()==revision && !map.getTileL(64,64,7),
			"Reservation constructor/destructor failure changed grid structure or metadata");
	}
	std::unique_ptr<Tile> incoming = std::make_unique<Tile>(64,64,7);
	auto* identity=incoming.get();
	{
		BaseMap::TileReservation reservation(map,positions);
		ordinal=0;failAt=1;observed=false;recording=true;
		map.swapReservedTile({64,64,7},incoming);
		map.swapReservedTile({64,64,7},incoming);
		recording=false;
		check(!observed && ordinal==0 && incoming.get()==identity && map.getTileCount()==1,"Reserved exchange allocated or lost ownership");
		reservation.commit();
	}
	check(map.getTileL(64,64,7) && map.getTileCount()==1,"Reservation commit did not retain infrastructure");
	report << "TERRAIN_RESERVATION_FAILURES_PASS\t" << allocations << '\n';
}

void terrainCommitSize(uint32_t side, std::ostream& report) {
	CommitSettings settings;
	check(g_gui.NewMap(),"Commit editor creation failed");
	auto& editor=*g_gui.GetCurrentEditor();
	const GenerationConfig config {{64,64,7,side,side},99887766,3,2,1};
	const Position selectedPosition(65,65,7), manualPosition(1100,1100,7);
	auto* original=editor.map.createTile(65,65,7);
	editor.selection.addInternal(original);
	const auto* grass=g_brushes.getBrush("grass")->as<GroundBrush>();
	check(grass && !grass->getBorderItems().empty(),"Commit grass fixture missing");
	const auto id=grass->getBorderItems().front().id;
	commitManualDraw(editor,manualPosition,id);
	const auto prior=editor.actionQueue->getCurrentIndex();
	check(prior==1,"Manual prefix not published");
	auto prepared=prepareTerrain(editor.map,config,profile,preparationBudget);
	check(bool(prepared),"Commit size preparation failed");
	const auto baselineCount=editor.map.getTileCount();
	const auto baselineRevision=editor.map.getChangeTracker().getGeneration();
	size_t publicationAllocations=0;
	const auto listener=editor.map.getChangeTracker().addListener([&](const auto&) {
		if (!editor.actionQueue->isApplyingTerrain()) return;
		publicationAllocations=ordinal; recording=false;
		check(editor.actionQueue->isApplyingTerrain(),"Terrain observer outside history guard");
		check(!editor.actionQueue->undo() && !editor.actionQueue->redo(),"Reentrant terrain history accepted");
		bool selectionRefused=false;
		try { editor.selection.start(); } catch (const std::logic_error&) { selectionRefused=true; }
		check(selectionRefused && !editor.selection.isBusy(),"Terrain observer started unpublished selection action");
	});
	startHeapMeasurement(); ordinal=0;failAt=SIZE_MAX;recording=true;
	const auto committed=DrawOperations::commitTerrainPreview(*prepared,config,profile,commitBudget);
	recording=false;heapRecording=false;
	const auto peak=heapPeak;
	check(committed && committed->changes==prepared->preview.getChanges().size() &&
		committed->previewFingerprint==prepared->preview.getFingerprint() &&
		committed->totalBytes==prepared->memory.totalBytes+committed->transitionBytes && peak<=committed->transitionBytes && !heapOverflow,
		"Commit failed or estimate below tracked new peak");
	check(editor.actionQueue->getCurrentIndex()==prior+1 && editor.actionQueue->getSize()==prior+1 &&
		editor.map.getChangeTracker().getGeneration()>baselineRevision && editor.map.hasChanged(),"Commit history/dirty state missing");
	checkCommitted(editor,*prepared);
	check(editor.selection.size()==1 && editor.selection.getSelectedTile()==editor.map.getTile(selectedPosition) &&
		!editor.selection.getSelectedTile()->isSelected(),"No-bit membership not transferred");
	check(editor.map.getTileCount()==baselineCount+committed->changes-1,"Commit tile count wrong");
	(void)g_minimap.TakePendingInvalidation(editor.map);
	const auto indexBefore=editor.actionQueue->getCurrentIndex();
	ordinal=0;failAt=1;observed=false;recording=true;
	const bool failedUndo=editor.actionQueue->undo();
	recording=false;
	check(!failedUndo && observed && editor.actionQueue->getCurrentIndex()==indexBefore,"Minimap preflight Undo fault escaped or changed history");
	checkCommitted(editor,*prepared);
	g_minimap.PrepareInvalidationSlot(editor.map);
	const auto historyBytes=committed->transitionBytes;
	const auto commitAllocations=publicationAllocations;
	for (int repeat=0;repeat<2;++repeat) {
		ordinal=0;failAt=1;observed=false;recording=true;
		check(editor.actionQueue->undo(),"Terrain Undo refused");
		recording=false;
		check(!observed && publicationAllocations==0 && commitSnapshotEqual(editor.map,prepared->snapshot) &&
			editor.map.getTileCount()==baselineCount && editor.selection.getSelectedTile()==original,"Terrain Undo changed source or allocated");
		if (repeat==0) {
			(void)g_minimap.TakePendingInvalidation(editor.map);
			ordinal=0;failAt=1;observed=false;recording=true;
			const bool failedRedo=editor.actionQueue->redo();
			recording=false;
			check(!failedRedo && observed && editor.actionQueue->getCurrentIndex()==prior &&
				commitSnapshotEqual(editor.map,prepared->snapshot),"Minimap preflight Redo fault escaped or mutated map");
			g_minimap.PrepareInvalidationSlot(editor.map);
		}
		ordinal=0;observed=false;recording=true;
		check(editor.actionQueue->redo(),"Terrain Redo refused");
		recording=false;
		check(!observed && publicationAllocations==0,"Terrain Redo allocated before publication");
		checkCommitted(editor,*prepared);
	}
	// A later ordinary draw gets its own entry even with action grouping enabled.
	const Position temporaryMember(64,64,7);
	editor.selection.start(Selection::INTERNAL);
	editor.selection.removeInternal(editor.map.getTile(selectedPosition));
	editor.selection.addInternal(editor.map.getTile(temporaryMember));
	editor.selection.finish(Selection::INTERNAL);
	size_t unchangedCountNotifications=0;
	editor.selection.addListener(987655,[&](size_t) { ++unchangedCountNotifications; });
	lua("g1_equal_selection_events=0;g1_equal_selection_listener=app.events:on('selectionChange', function() g1_equal_selection_events=g1_equal_selection_events+1 end)");
	check(editor.actionQueue->undo() && editor.selection.size()==1 && editor.selection.getSelectedTile()==original,
		"Same-count terrain Undo membership exchange failed");
	lua("assert(g1_equal_selection_events==1)");
	check(editor.actionQueue->redo() && editor.selection.size()==1 &&
		editor.selection.getSelectedTile()==editor.map.getTile(temporaryMember),"Same-count terrain Redo membership exchange failed");
	lua("assert(g1_equal_selection_events==2);app.events:off(g1_equal_selection_listener);g1_equal_selection_listener=nil");
	check(unchangedCountNotifications==0,"Same-count membership change emitted a false count notification");
	editor.selection.removeListener(987655);
	editor.selection.start(Selection::INTERNAL);
	editor.selection.removeInternal(editor.map.getTile(temporaryMember));
	editor.selection.addInternal(editor.map.getTile(selectedPosition));
	editor.selection.finish(Selection::INTERNAL);
	commitManualDraw(editor,manualPosition,id);
	check(editor.actionQueue->getCurrentIndex()==prior+2,"Following draw merged into generator");
	check(editor.actionQueue->undo() && editor.actionQueue->undo() &&
		commitSnapshotEqual(editor.map,prepared->snapshot),"Draw/generator independent Undo failed");
	check(editor.actionQueue->redo(),"Terrain redo after following draw failed");
	// A normal selection action must restore collection membership without relying on selected bits.
	editor.selection.start();
	editor.selection.remove(editor.map.getTile(selectedPosition));
	editor.selection.finish();
	check(editor.selection.empty(),"Selection removal did not remove terrain member");
	check(editor.actionQueue->undo() && editor.selection.size()==1 &&
		editor.selection.getSelectedTile()==editor.map.getTile(selectedPosition),"Selection Undo lost unflagged membership");
	check(editor.actionQueue->undo() && editor.selection.getSelectedTile()==original,"Terrain Undo after selection Undo dangling");
	editor.map.getChangeTracker().removeListener(listener);
	heapOwner=false;
	editor.actionQueue->clear();editor.selection.clear();editor.map.clearChanges();
	check(g_gui.CloseAllEditors(),"Commit size editors did not close");
	report << "TERRAIN_COMMIT_SIZE_PASS\t" << side << '\t' << historyBytes << '\t' << peak << '\t'
		<< commitAllocations << '\t' << prepared->preview.getFingerprint() << '\n';
}

class CommitProbeQueue final : public ActionQueue {
public:
	explicit CommitProbeQueue(Editor& editor):ActionQueue(editor) {}
	struct State {
		size_t index,memory;
		std::vector<const BatchAction*> identities;
		std::vector<std::string> labels;
		bool operator==(const State&) const = default;
	};
	void freshStorage() { clear(); ActionList fresh; actions.swap(fresh); }
	State state() const {
		State result {current,memory_size,{},{}};
		for (const auto& entry:actions) { result.identities.push_back(entry.get());result.labels.push_back(entry->getLabel()); }
		return result;
	}
};

bool commitFailedAs(const std::expected<TerrainCommitReceipt,TerrainCommitError>& result,TerrainCommitFailure error) {
	return !result && std::get_if<TerrainCommitFailure>(&result.error()) && std::get<TerrainCommitFailure>(result.error())==error;
}

void terrainCommitFailures(std::ostream& report) {
	CommitSettings settings;
	check(g_gui.NewMap(),"Commit fault editor creation failed");
	auto& editor=*g_gui.GetCurrentEditor();
	editor.actionQueue=std::make_unique<CommitProbeQueue>(editor);
	auto& queue=static_cast<CommitProbeQueue&>(*editor.actionQueue);
	const GenerationConfig config {{63,63,7,4,4},123456,1,0,0};
	auto* original=editor.map.createTile(64,64,7);
	editor.selection.addInternal(original);
	const auto id=g_brushes.getBrush("grass")->as<GroundBrush>()->getBorderItems().front().id;
	commitManualDraw(editor,{1100,1100,7},id);
	commitManualDraw(editor,{1101,1101,7},id);
	check(queue.undo() && queue.canRedo(),"Commit fault Redo fixture failed");
	auto prepared=prepareTerrain(editor.map,config,profile,preparationBudget);
	check(bool(prepared),"Commit fault preparation failed");
	const auto listener=editor.map.getChangeTracker().addListener([&](const auto&) {
		if (queue.isApplyingTerrain()) recording=false; // End measurement at already published state.
	});
	ordinal=0;failAt=SIZE_MAX;recording=true;
	const auto successful=DrawOperations::commitTerrainPreview(*prepared,config,profile,commitBudget);
	recording=false;
	const auto allocations=ordinal;
	check(successful && queue.undo(),"Commit fault baseline failed");
	// Restore a fresh deque as well, so the sweep reaches actual history storage growth.
	queue.freshStorage();
	commitManualDraw(editor,{1100,1100,7},id);
	commitManualDraw(editor,{1101,1101,7},id);
	check(queue.undo(),"Commit fault Redo restoration failed");
	prepared=prepareTerrain(editor.map,config,profile,preparationBudget);
	check(bool(prepared),"Commit fault refreshed preparation failed");
	size_t faults=0;
	for (size_t fault=1;fault<=allocations;++fault) {
		const auto state=queue.state();
		const auto revision=editor.map.getChangeTracker().getGeneration();
		const auto count=editor.map.getTileCount();
		const auto cellCount=editor.map.getGrid().cellCount();
		const auto dirty=editor.map.hasChanged();
		ordinal=0;failAt=fault;observed=false;recording=true;
		const auto result=DrawOperations::commitTerrainPreview(*prepared,config,profile,commitBudget);
		recording=false;
		if (observed) {
			++faults;
			check(commitFailedAs(result,TerrainCommitFailure::AllocationFailed) && queue.state()==state &&
				editor.map.getChangeTracker().getGeneration()==revision && editor.map.hasChanged()==dirty &&
				editor.map.getTileCount()==count && editor.map.getGrid().cellCount()==cellCount &&
				editor.selection.getSelectedTile()==original && snapshotMatchesMap(editor.map,prepared->snapshot),
				"Failed terrain allocation changed map, structure, selection, revision or history");
			check(queue.redo() && queue.undo(),"Original Redo/Undo unusable after failed commit");
		} else {
			check(result && queue.undo(),"Unexpected refusal outside fault boundary");
			// Recreate one actual Redo entry outside the terrain area.
			commitManualDraw(editor,{1101,1101,7},id);
			check(queue.undo(),"Redo after successful sweep case failed");
		}
		check(commitSnapshotEqual(editor.map,prepared->snapshot),"Fault sweep source contents changed");
		queue.freshStorage();
		commitManualDraw(editor,{1100,1100,7},id);
		commitManualDraw(editor,{1101,1101,7},id);
		check(queue.undo(),"Fresh history fixture restoration failed");
		prepared=prepareTerrain(editor.map,config,profile,preparationBudget);
		check(bool(prepared),"Fault sweep reprepare failed");
	}
	const auto unchanged=[&](const auto& result) {
		check(!result && snapshotMatchesMap(editor.map,prepared->snapshot) && editor.selection.getSelectedTile()==original,
			"Commit refusal mutated its input");
	};
	unchanged(DrawOperations::commitTerrainPreview(*prepared,config,profile,{}));
	auto limit=commitBudget;limit.maxChanges=15;
	check(commitFailedAs(DrawOperations::commitTerrainPreview(*prepared,config,profile,limit),TerrainCommitFailure::ChangeLimit),"Commit change limit ignored");
	limit=commitBudget;limit.maxBytes=successful->totalBytes-1;
	unchanged(DrawOperations::commitTerrainPreview(*prepared,config,profile,limit));
	std::stop_source stop;stop.request_stop();
	unchanged(DrawOperations::commitTerrainPreview(*prepared,config,profile,commitBudget,{},stop.get_token()));
	bool workerRefused=false;
	std::jthread worker([&] { workerRefused=!DrawOperations::commitTerrainPreview(*prepared,config,profile,commitBudget); });
	worker.join();check(workerRefused,"Commit accessed UI from worker");
	bool luaRefused=false;
	auto& state=g_luaScripts.getEngine().getState();
	state["g1_commit_in_transaction"]=[&] {
		luaRefused=commitFailedAs(DrawOperations::commitTerrainPreview(*prepared,config,profile,commitBudget),TerrainCommitFailure::ActiveLuaTransaction);
	};
	lua("app.transaction('commit guard', function() g1_commit_in_transaction() end)");
	state["g1_commit_in_transaction"]=sol::nil;
	check(luaRefused,"Commit accepted active Lua transaction");
	g_settings.setInteger(Config::UNDO_MEM_SIZE,0);
	check(commitFailedAs(DrawOperations::commitTerrainPreview(*prepared,config,profile,commitBudget),TerrainCommitFailure::HistoryRefused),"Commit ignored disabled Undo");
	g_settings.setInteger(Config::UNDO_MEM_SIZE,512);
	const auto final=DrawOperations::commitTerrainPreview(*prepared,config,profile,commitBudget);
	check(final && final->totalBytes>=successful->totalBytes,"Commit retry estimate invalid");
	bool luaUndoRefused=false;
	state["g1_undo_in_transaction"]=[&] { luaUndoRefused=!queue.undo(); };
	lua("app.transaction('undo guard', function() g1_undo_in_transaction() end)");
	state["g1_undo_in_transaction"]=sol::nil;
	check(luaUndoRefused && queue.undo() && commitSnapshotEqual(editor.map,prepared->snapshot),"Terrain Undo transaction guard or retry failed");
	prepared=prepareTerrain(editor.map,config,profile,preparationBudget);
	check(bool(prepared),"Exact commit budget reprepare failed");
	limit=commitBudget;limit.maxBytes=final->totalBytes-1;
	check(commitFailedAs(DrawOperations::commitTerrainPreview(*prepared,config,profile,limit),TerrainCommitFailure::MemoryLimit),"One-byte-short commit budget accepted");
	++limit.maxBytes;
	check(DrawOperations::commitTerrainPreview(*prepared,config,profile,limit) && queue.undo(),"Exact commit budget refused");
	editor.map.getChangeTracker().removeListener(listener);
	queue.clear();editor.selection.clear();editor.map.clearChanges();
	check(g_gui.CloseAllEditors(),"Commit fault editor did not close");
	report << "TERRAIN_COMMIT_FAILURES_PASS\t" << faults << '\t' << allocations << '\n';
}

void terrainSelectedCommit(std::ostream& report) {
	CommitSettings settings;
	check(g_gui.NewMap(),"Selected commit editor creation failed");
	auto& editor=*g_gui.GetCurrentEditor();
	const GenerationConfig config {{200,200,7,4,4},112233,1,0,0};
	const std::array<Position,5> positions {{{200,200,7},{203,200,7},{200,203,7},{203,203,7},{201,201,7}}};
	std::array<Tile*,5> original {};
	for (size_t i=0;i<positions.size();++i) {
		original[i]=editor.map.createTile(positions[i].x,positions[i].y,7);
		editor.selection.addInternal(original[i]);
	}
	auto* hole=editor.map.createTile(202,202,7);
	const auto selection=captureGenerationSelection(editor,{16,1024});
	check(bool(selection),"Selected commit mask capture failed");
	auto prepared=prepareTerrain(editor.map,config,profile,preparationBudget,selection->mask);
	check(prepared && prepared->preview.getChanges().size()==5,"Selected commit preparation failed");
	editor.selection.removeInternal(original.back());editor.selection.addInternal(hole);
	check(!DrawOperations::commitSelectedTerrainPreview(*prepared,*selection,config,profile,commitBudget) &&
		editor.actionQueue->getSize()==0,"Selected commit accepted changed same-bounds membership");
	editor.selection.removeInternal(hole);editor.selection.addInternal(original.back());
	const auto accounting=prepared->memory;
	prepared->memory={};
	auto tiny=commitBudget;tiny.maxBytes=1;
	check(commitFailedAs(DrawOperations::commitSelectedTerrainPreview(*prepared,*selection,config,profile,tiny),
		TerrainCommitFailure::MemoryLimit),"Commit trusted forged public memory accounting");
	prepared->memory=accounting;prepared->memory.totalBytes=UINT64_MAX;
	tiny.maxBytes=UINT64_MAX;
	check(commitFailedAs(DrawOperations::commitSelectedTerrainPreview(*prepared,*selection,config,profile,tiny),
		TerrainCommitFailure::MemoryLimit),"Joint commit accounting overflow accepted");
	prepared->memory=accounting;
	editor.selection.start(Selection::INTERNAL);
	check(!DrawOperations::commitSelectedTerrainPreview(*prepared,*selection,config,profile,commitBudget),"Selected commit accepted busy selection");
	editor.selection.finish(Selection::INTERNAL);
	editor.live_manager.StartServer();
	check(!DrawOperations::commitSelectedTerrainPreview(*prepared,*selection,config,profile,commitBudget),"Selected commit accepted inert live server");
	editor.live_manager.CloseServer();
	check(DrawOperations::commitSelectedTerrainPreview(*prepared,*selection,config,profile,commitBudget) &&
		editor.actionQueue->getCurrentIndex()==1 && editor.map.getTile(202,202,7)==hole,"Selected commit changed mask hole");
	checkCommitted(editor,*prepared);
	editor.selection.start(Selection::INTERNAL);
	check(!editor.actionQueue->undo() && editor.actionQueue->getCurrentIndex()==1,"Terrain Undo accepted busy selection");
	editor.selection.finish(Selection::INTERNAL);
	// The actionChange listener makes a normal undoable selection edit after publication.
	bool callbackRan=false,guardReleased=false;
	auto& luaState=g_luaScripts.getEngine().getState();
	luaState["g1_commit_after_event"]=[&] {
		if (callbackRan) return;
		callbackRan=true;guardReleased=!editor.actionQueue->isApplyingTerrain();
		editor.selection.start();editor.selection.remove(editor.map.getTile(200,200,7));editor.selection.finish();
	};
	lua("g1_commit_listener=app.events:on('actionChange', function() g1_commit_after_event() end)");
	const GenerationConfig nextConfig {{300,300,7,4,4},445566,1,0,0};
	const auto next=prepareTerrain(editor.map,nextConfig,profile,preparationBudget);
	check(next && DrawOperations::commitTerrainPreview(*next,nextConfig,profile,commitBudget),"Second terrain commit failed");
	lua("app.events:off(g1_commit_listener);g1_commit_listener=nil");luaState["g1_commit_after_event"]=sol::nil;
	check(callbackRan && guardReleased && editor.actionQueue->getCurrentIndex()==3 && editor.selection.size()==4,
		"Post-commit Lua selection edit was blocked or silently lost from history");
	check(editor.actionQueue->undo() && editor.selection.size()==5,"Callback selection Undo failed");
	check(editor.actionQueue->undo() && commitSnapshotEqual(editor.map,next->snapshot),"Two terrain commits merged");
	check(editor.actionQueue->undo() && commitSnapshotEqual(editor.map,prepared->snapshot),"Selected terrain Undo changed source");
	for (size_t i=0;i<positions.size();++i) check(editor.map.getTile(positions[i])==original[i] &&
		editor.selection.containsExact(original[i]),"Selected Undo lost original identity or membership");
	check(editor.actionQueue->redo() && editor.actionQueue->redo(),"Two terrain Redos failed");
	checkCommitted(editor,*next);
	// A history-less member at a formerly null position must disappear safely on Undo.
	editor.selection.addInternal(editor.map.getTile(300,300,7));
	size_t countNotifications=0;
	editor.selection.addListener(987654,[&](size_t count) { check(count==5,"Wrong terrain selection notification count");++countNotifications; });
	lua("g1_commit_selection_count=0;g1_commit_selection_listener=app.events:on('selectionChange', function() g1_commit_selection_count=g1_commit_selection_count+1 end)");
	check(editor.actionQueue->undo() && editor.selection.size()==5 && countNotifications==1,"Null-source Undo membership/count notification failed");
	lua("assert(g1_commit_selection_count==1);app.events:off(g1_commit_selection_listener);g1_commit_selection_listener=nil");
	editor.selection.removeListener(987654);
	check(editor.actionQueue->redo() && editor.selection.size()==6 &&
		editor.selection.containsExact(editor.map.getTile(300,300,7)) && !editor.map.getTile(300,300,7)->isSelected(),
		"Null-source Redo lost unflagged membership");
	TileOperations::select(editor.map.getTile(300,300,7));
	check(editor.map.getTile(300,300,7)->isSelected() && editor.actionQueue->undo() &&
		editor.actionQueue->redo() && editor.selection.containsExact(editor.map.getTile(300,300,7)) &&
		editor.map.getTile(300,300,7)->isSelected(),"Null-source Redo lost flagged membership or changed item flags");
	check(editor.actionQueue->undo(),"Null-source final Undo failed");
	// Shrink the public collection via its ordinary deferred flush, then force allocation during
	// restoration of the original no-bit members. The entire failure precedes tile/history swaps.
	editor.selection.start(Selection::INTERNAL);editor.selection.clear();editor.selection.finish(Selection::INTERNAL);
	editor.selection.start(Selection::INTERNAL);editor.selection.addInternal(hole);editor.selection.finish(Selection::INTERNAL);
	g_minimap.PrepareInvalidationSlot(editor.map);
	const auto indexBeforeSelectionFault=editor.actionQueue->getCurrentIndex();
	ordinal=0;failAt=1;observed=false;recording=true;
	const bool selectionFaultUndo=editor.actionQueue->undo();
	recording=false;
	check(!selectionFaultUndo && observed && editor.actionQueue->getCurrentIndex()==indexBeforeSelectionFault &&
		editor.selection.size()==1 && editor.selection.getSelectedTile()==hole,"Selection reserve fault changed terrain history/membership");
	checkCommitted(editor,*prepared);
	check(editor.actionQueue->undo() && editor.selection.size()==6 && editor.actionQueue->redo() &&
		editor.selection.size()==1 && editor.selection.getSelectedTile()==hole,"Saved no-bit membership restoration failed after collection shrink");
	editor.selection.start(Selection::INTERNAL);editor.selection.clear();editor.selection.finish(Selection::INTERNAL);
	editor.selection.start(Selection::INTERNAL);
	for (int i=0;i<11;++i) editor.selection.addInternal(editor.map.createTile(700+i,700,7));
	editor.selection.finish(Selection::INTERNAL);
	check(!editor.actionQueue->undo() && editor.selection.size()==11 &&
		editor.actionQueue->getCurrentIndex()==indexBeforeSelectionFault,"Undo exceeded its reserved selection allocation budget");
	editor.selection.start(Selection::INTERNAL);editor.selection.clear();editor.selection.finish(Selection::INTERNAL);
	editor.selection.start(Selection::INTERNAL);editor.selection.addInternal(hole);editor.selection.finish(Selection::INTERNAL);
	check(editor.actionQueue->undo() && editor.selection.size()==6,"Undo selection budget retry failed");
	editor.actionQueue->clear();editor.selection.clear();editor.map.clearChanges();
	check(g_gui.CloseAllEditors(),"Selected commit editor did not close");
	report << "TERRAIN_SELECTED_COMMIT_PASS\t5\t2\t1\n";
	report << "TERRAIN_SELECTION_HISTORY_PASS\t2\t1\t1\n";
}

void run() {
	const auto root = std::filesystem::current_path();
	std::ofstream report(root/"resolver-probe.log");
	report << std::unitbuf;
	try {
		check(std::filesystem::is_regular_file(root/".g1-resolver-probe"), "Private probe required");
		g_version.UnloadVersion();
		const auto missing = DrawOperations::prepareTerrainPreview(Map{}, {}, {}, previewBudget);
		check(!missing && missing.error() == PreviewError::NoClient, "No client accepted");
		const auto missingPreparation = prepareTerrain(Map{}, {}, profile, preparationBudget);
		check(failedAs(missingPreparation,PreparationInputError::NoClient), "Joint preparation accepted missing client");
		const auto missingAssets = fingerprintGenerationAssets();
		check(!missingAssets && missingAssets.error() == AssetFingerprintError::NoClient, "Asset fingerprint accepted missing client");
		wxString error;
		std::vector<std::string> warnings;
		check(g_version.LoadVersion("1098-BlackTek",error,warnings), "Private client load failed");
		createObservers(report);
		detachedNotifications(report);
		oneSize(100,false,report);
		oneSize(100,true,report);
		oneSize(500,false,report);
		oneSize(500,true,report);
		failures(report);
		excluded(report);
		selectedContext(report);
		selectedBorderRefusal(report);
		liveSelectionAndLua(report);
		baseGroundVariant(report);
		preparationSize(100,report);
		preparationSize(500,report);
		preparationFailures(report);
		preparedValidationFailures(report);
		exactSelectionFailures(report);
		exactSelectionSize(100,report);
		exactSelectionSize(500,report);
		terrainTargetSize(100,report);
		terrainTargetSize(500,report);
		generationAssetMutations(report);
		closeObservers();
		tileReservationFailures(report);
		terrainCommitFailures(report);
		terrainSelectedCommit(report);
		terrainCommitSize(100,report);
		terrainCommitSize(500,report);
		terrainTargetLifecycle(report);
		irregularAndEpoch(report);
		report << "PREVIEW_PASS\n";
		report.close();
		wxTheApp->CallAfter([] { g_gui.root->Close(); });
	} catch (const std::exception& error) {
		recording = false;
		heapOwner = heapRecording = false;
		report << "FAILED\t" << error.what() << '\n';
		report.close();
		std::_Exit(9);
	}
}
}
class G1PreviewModule : public wxModule {
public:
	bool OnInit() override { wxTheApp->Bind(wxEVT_IDLE,&G1PreviewModule::OnIdle,this); return true; }
	void OnExit() override {}
	void OnIdle(wxIdleEvent& event) {
		event.Skip();
		if (!g_gui.root || !wxApp::IsMainLoopRunning() || !g_gui.root->IsEnabled()) return;
		wxTheApp->Unbind(wxEVT_IDLE,&G1PreviewModule::OnIdle,this);
		wxTheApp->CallAfter([] { run(); });
	}
private:
	wxDECLARE_DYNAMIC_CLASS(G1PreviewModule);
};
wxIMPLEMENT_DYNAMIC_CLASS(G1PreviewModule,wxModule);
