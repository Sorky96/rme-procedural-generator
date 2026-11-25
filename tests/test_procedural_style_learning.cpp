// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/style_learning.h"
#include "procedural/core/doodad_class.h"
#include "procedural/core/style_profile_set.h"
#include "procedural/core/style_history.h"
#include "procedural/core/candidate_series.h"
#include <fstream>
#include <iterator>
#include <optional>
#include <iostream>

using namespace rme::procedural;
namespace {
int failures = 0;
void check(bool ok, const char* message) { if (!ok) { ++failures; std::cerr << "FAIL: " << message << '\n'; } }
SemanticMap fixture(int natural, int unknown = 0) {
    FloorBounds b{7, 10, 20, 64, 64};
    auto m = SemanticMap::create(std::span(&b, 1), {4096}).value();
    const auto grass = m.internGroundKey("grass").value();
    const auto mystery = m.internGroundKey("unrecognized terrain").value();
    for (int i = 0; i < natural + unknown; ++i)
        check(bool(m.set(10 + i % 64, 20 + i / 64, 7, i < natural ? grass : mystery, bit(Trait::HasGround))), "fixture cell");
    return m;
}
}
int main(int argc, char** argv) {
    auto m = fixture(1024, 1024);
    auto mapping = proposeKeyMap(m.groundKeys(), defaultKeyRules());
    check(mapping.back().kind == StyleKind::Unmapped, "unknown names not guessed");
    auto badMapping = mapping; badMapping[0].kind = StyleKind::Grass;
    check(!inspectLearningSample(m, badMapping, {}, true), "unresolved ground cannot become natural");
    badMapping = mapping; badMapping[1].kind = static_cast<StyleKind>(255);
    check(!inspectLearningSample(m, badMapping, {}, true), "invalid class refused safely");
    check(!inspectLearningSample(m, mapping, {}, false), "permission required");
    const auto sample = inspectLearningSample(m, mapping, {}, true);
    check(sample && sample->recognized == 1024 && sample->present == 2048, "inclusive minimum sample/50 percent");
    auto small = fixture(1023);
    check(!inspectLearningSample(small, proposeKeyMap(small.groundKeys(), defaultKeyRules()), {}, true), "1023 refused");
    auto low = fixture(1024, 1025);
    check(!inspectLearningSample(low, mapping, {}, true), "below 50 percent refused");
    auto unknown = fixture(0, 2048);
    check(!inspectLearningSample(unknown, mapping, {}, true), "all unknown refused");
    mapping.back().kind = StyleKind::Dirt;
    check(inspectLearningSample(m, mapping, {}, true)->recognized == 2048, "reviewed correction used");
    mapping.back().kind = StyleKind::Unmapped;
    std::stop_source cancelled; cancelled.request_stop();
    check(!inspectLearningSample(m, mapping, {}, true, cancelled.get_token()), "cancel sample");
    check(!fitLearningSample(*sample, mapping, "10.98", "otb", "dat", cancelled.get_token()), "cancel fit");
    const auto a = fitLearningSample(*sample, mapping, "10.98", "otb", "dat");
    const auto b = fitLearningSample(*sample, mapping, "10.98", "otb", "dat");
    check(a && b && *a == *b, "deterministic G4 fit");
    if (!a) return 1;
    check(a->tuning && a->tuning->rockPermille == 0 && a->tuning->dirtPermille == 0, "kinds absent from the sample are not generated");
    check(a->tuning->lakePermille == 0, "no water in the sample, no lakes");
    check(a->palette[size_t(StyleKind::Shore)] == a->palette[size_t(StyleKind::Grass)], "shoreless sample rings water with its grass");
    {
        // Every grass tile holds a tree, every swamp tile a swamp plant: the learned profile keeps the palm
        // brush, a vegetation fit, and swamp patches without trees that grow their own plants.
        FloorBounds fb{7, 0, 0, 64, 64};
        auto forest = SemanticMap::create(std::span(&fb, 1), {4096}).value();
        const auto grass = forest.internGroundKey("grass").value();
        const auto jungle = forest.internGroundKey("jungle grass").value();
        const auto swamp = forest.internGroundKey("swamp").value();
        const auto sea = forest.internGroundKey("sea").value(); // no tiles; RAW items on it stand on water
        for (int i = 0; i < 4096; ++i) // a quarter jungle grass, an eighth swamp
            check(bool(forest.set(i % 64, i / 64, 7, i % 8 == 1 ? swamp : i % 4 ? grass : jungle,
                bit(Trait::HasGround) | bit(i % 8 == 1 ? Trait::BushDoodad : Trait::TreeDoodad))), "forest cell");
        auto forestMapping = proposeKeyMap(forest.groundKeys(), defaultKeyRules());
        for (auto& entry : forestMapping) if (entry.key == "swamp") entry.kind = StyleKind::Grass; // as the dialog proposes
        const std::vector<GroundDoodadItems> palms{{grass, "mangrove tree (on water)", 9000}, {grass, "jungle palm", 1300},
            {grass, "oak tree", 1200}, {grass, "pine tree", 1100}, {grass, "birch tree", 1000}, {grass, "fir tree", 900},
            {swamp, "swamp plants", 512}, {grass, "grass tufts", 600}, {grass, "tall jungle grass", 300}, {grass, "bushes", 100}, {sea, "raw:tree:4000", 50000}};
        // Cobwebs on 400 of the 3584 trees (in parts per million), none on the 512 swamp tiles; 1 of anything is too few, and on water never.
        const std::vector<GroundOverlayItems> webs{{grass, "raw:item:999", 400, 0}, {grass, "raw:item:998", 1, 0}, {sea, "raw:item:997", 900, 900}};
        const auto learned = inspectLearningSample(forest, forestMapping, palms, true, {}, webs);
        check(learned && learned->overlays[0] == StyleOverlay{"raw:item:999", 111607, 0} && learned->overlays[1].name.empty(), "overlays by use on land");
        check(learned && learned->decorPalette[0] == "jungle palm", "tree brush learned, the RAW tree on water left out");
        check(learned && learned->decorPalette[1] == "grass tufts", "plants of the swamp patches are not the main bush");
        check(learned && learned->bushMix[0] == StyleMix{"tall jungle grass", 300} && learned->bushMix[1] == StyleMix{"bushes", 100},
            "bush mix by use");
        check(learned && learned->grassMixDecor[1].keepPermille == std::array<uint32_t, 3>{0, 1000, 1000} &&
            learned->grassMixDecor[1].brushes[1] == "swamp plants", "swamp keeps no trees and grows its own plants");
        check(learned && learned->grassMix[0] == StyleMix{"jungle grass", 250} && learned->grassMix[1] == StyleMix{"swamp", 125} &&
            learned->grassMix[2].brush.empty(), "grass mix with shares");
        // 5500 land trees: oak 1200 (218), pine 1100 (200), birch 1000 (181); fir (163) has no slot left.
        check(learned && learned->treeMix[0] == StyleMix{"oak tree", 218} && learned->treeMix[1] == StyleMix{"pine tree", 200} &&
            learned->treeMix[2] == StyleMix{"birch tree", 181}, "tree mix by use, water trees left out");
        const auto fitted = learned ? fitLearningSample(*learned, forestMapping, "10.98", "otb", "dat")
                                    : std::expected<StyleProfile, std::string>(std::unexpected("no sample"));
        check(fitted && fitted->decorTuning && fitted->decorTarget && fitted->decorPalette[0] == "jungle palm", "vegetation fitted");
        check(fitted && parseProfile(*serializeProfile(*fitted)) == *fitted, "learned vegetation and mixes round-trip");
        if (fitted) {
            auto broken = *fitted;
            broken.treeMix[0].permille = 900; // 900 + 200 + 181 > 1000
            check(!validateProfile(broken), "tree mix shares above 1000 refused");
            broken = *fitted;
            broken.grassMix[0].brush.clear();
            check(!validateProfile(broken), "grass variant share without a brush refused");
        }
    }
    {
        // Vegetation in 8 x 8 thickets on a checkerboard is clumped; every other column planted is not.
        const auto clumpOf = [](auto planted) {
            FloorBounds fb{7, 0, 0, 64, 64};
            auto map = SemanticMap::create(std::span(&fb, 1), {4096}).value();
            const auto grass = map.internGroundKey("grass").value();
            for (int i = 0; i < 4096; ++i)
                check(bool(map.set(i % 64, i / 64, 7, grass, bit(Trait::HasGround) | (planted(i % 64, i / 64) ? bit(Trait::BushDoodad) : 0))), "clump cell");
            const auto sample = inspectLearningSample(map, proposeKeyMap(map.groundKeys(), defaultKeyRules()), {}, true);
            return sample ? std::optional(sample->clumpPermille) : std::nullopt;
        };
        check(clumpOf([](int x, int y) { return (x / 8 + y / 8) % 2 == 0; }) > 250u, "thickets are clumped");
        check(clumpOf([](int x, int) { return x % 2 == 0; }) == 0u, "an even spread is not clumped");
    }
    {
        // A 32 x 32 island with a 4 x 4 pond in a 64 x 64 sea: the sea is learned as sea, the pond as lakes.
        FloorBounds fb{7, 0, 0, 64, 64};
        auto map = SemanticMap::create(std::span(&fb, 1), {4096}).value();
        const auto grass = map.internGroundKey("grass").value();
        const auto sea = map.internGroundKey("sea").value();
        for (int i = 0; i < 4096; ++i) {
            const int x = i % 64, y = i / 64;
            const bool land = x >= 16 && x < 48 && y >= 16 && y < 48 && !(x >= 30 && x < 34 && y >= 30 && y < 34);
            check(bool(map.set(x, y, 7, land ? grass : sea, bit(Trait::HasGround) | (land ? 0 : bit(Trait::TreeDoodad)))), "island cell");
        }
        const auto islandMapping = proposeKeyMap(map.groundKeys(), defaultKeyRules());
        // Every water tile holds a mangrove: all of the bank does.
        const std::vector<GroundDoodadItems> mangroves{{sea, "mangrove tree (on water)", 3088}};
        const auto islandSample = inspectLearningSample(map, islandMapping, mangroves, true);
        check(islandSample && islandSample->shoreTreePermille == 1000 && islandSample->shoreTree == "mangrove tree (on water)", "trees in the water by the bank");
        check(islandSample && islandSample->seaTiles == 4096 - 1024 &&
            islandSample->shoreDistance[0] >= 1 && islandSample->shoreDistance[0] <= islandSample->shoreDistance[1] &&
            islandSample->shoreDistance[1] <= islandSample->shoreDistance[3] && islandSample->shoreDistance[3] <= 16,
            "sea told from the pond");
        const auto island = islandSample ? fitLearningSample(*islandSample, islandMapping, "10.98", "otb", "dat")
                                         : std::expected<StyleProfile, std::string>(std::unexpected("no sample"));
        check(island && island->tuning->islandPermille == 750 && island->tuning->lakePermille == 16,
            "sea and lakes learned apart");
        check(island && parseProfile(*serializeProfile(*island)) == *island, "island round-trips");
    }
    {
        // A desert: sand everywhere, a little swamp, two walled ruins of sandstone floor (one with half its outline walled)
        // and a patch of cave floor with rock walls, which is the mountain's, not a building.
        FloorBounds fb{7, 0, 0, 64, 64};
        auto map = SemanticMap::create(std::span(&fb, 1), {4096}).value();
        const auto sand = map.internGroundKey("sand").value();
        const auto swamp = map.internGroundKey("swamp").value();
        const auto floorKey = map.internGroundKey("sandstone floor").value();
        const auto caveKey = map.internGroundKey("cave").value();
        for (int i = 0; i < 4096; ++i) {
            const int x = i % 64, y = i / 64;
            const bool house = x >= 10 && x < 15 && y >= 10 && y < 15, ruin = x >= 30 && x < 35 && y >= 10 && y < 15;
            const bool cavern = x >= 50 && x < 55 && y >= 50 && y < 55;
            const bool outline = (x == 10 || x == 14 || y == 10 || y == 14) || (ruin && (x == 30 || x == 34) && y >= 10);
            const bool walledRuin = ruin && (x == 30 || x == 34) && (y == 10 || y == 11 || y == 12);
            const uint16_t key = house || ruin ? floorKey : cavern ? caveKey : i % 50 == 0 ? swamp : sand;
            const uint32_t walls = (house && outline) || walledRuin || (cavern && (x == 50 || y == 50)) ? bit(Trait::Wall) : 0;
            check(bool(map.set(x, y, 7, key, bit(Trait::HasGround) | walls)), "desert cell");
        }
        std::vector<KeyRule> rules{{"swamp", StyleKind::Grass}};
        rules.insert(rules.end(), defaultKeyRules().begin(), defaultKeyRules().end());
        auto desert = proposeKeyMap(map.groundKeys(), rules);
        promoteMainLand(desert, map);
        check(std::ranges::find(desert, std::string("sand"), &KeyMapEntry::key)->kind == StyleKind::Grass, "sand is the main land of a desert");
        const std::vector<GroundDoodadItems> walls{{floorKey, "limestone wall", 20}, {caveKey, "rock wall", 30}};
        // Rubble on 10 of the 50 building floor tiles; the rubble on sand outside is no interior.
        const std::vector<GroundOverlayItems> rubble{{floorKey, "raw:item:42", 0, 10}, {sand, "raw:item:43", 0, 30}};
        const auto learned = inspectLearningSample(map, desert, {}, true, {}, rubble, walls);
        check(learned && learned->interiors[0] == StyleOverlay{"raw:item:42", 0, 200000} && learned->interiors[1].name.empty(), "rubble inside buildings");
        check(learned && learned->buildings.density == 32 && learned->buildings.ruinPercent == 50 &&
            learned->buildings.floor == "sandstone floor" && learned->buildings.wall == "limestone wall", "buildings and ruins, not caves");
        // Vertical wall tiles (wall pieces north and south): 3 on each house side and one in each ruin wall, 8 in all. A stack
        // of two items stands on 3 of them, another only once (dropped); a blocking stack inside the house reaches no wall
        // and one in the cave is the mountain's. The stack's items on the open sand are no longer loose overlays.
        std::vector<PlacedStack> placed;
        const auto at = [](int x, int y) { return uint32_t(y * 64 + x); };
        for (const auto [x, y] : {std::pair{10, 11}, {10, 12}, {14, 12}}) placed.push_back({at(x, y), {50, 60}, true});
        placed.push_back({at(14, 11), {70}, true});
        placed.push_back({at(12, 12), {80}, true});
        placed.push_back({at(51, 51), {50, 60}, true});
        const std::vector<GroundOverlayItems> loose{{sand, "raw:item:50", 0, 5}, {sand, "raw:item:90", 0, 2}};
        const auto decorated = inspectLearningSample(map, desert, {}, true, {}, loose, walls, placed);
        check(decorated && decorated->wallStacks == std::vector<StyleWallStack>{{5, 375000, {50, 60}}}, "wall stacks by sides");
        check(decorated && decorated->overlays[0].name == "raw:item:90" && decorated->overlays[1].name.empty(), "wall stack items are no loose overlays");
        // Drifted sand laid with a carpet brush on 25 of the 50 floor tiles; the same carpet on sand outside does not count.
        const std::vector<GroundDoodadItems> carpets{{floorKey, "sand gravel", 25}, {sand, "sand gravel", 40}, {floorKey, "rug", 2}};
        const auto carpeted = inspectLearningSample(map, desert, {}, true, {}, {}, walls, {}, carpets);
        check(carpeted && carpeted->carpet == StyleOverlay{"sand gravel", 0, 500000}, "carpet of the buildings");
        auto both = decorated ? *decorated : LearningSample{};
        if (carpeted) both.carpet = carpeted->carpet;
        both.rockOutlinePermille = 400;
        const auto withDecor = decorated ? fitLearningSample(both, desert, "10.98", "otb", "dat") : std::unexpected(std::string("no sample"));
        check(withDecor && parseProfile(*serializeProfile(*withDecor)) == *withDecor, "wall stacks, carpet and rock outline round-trip");
    }
    {
        // A wreck by the shore of a 48 x 48 sample (sea for x < 10): hull stacks on rows 10 and 13 (x 8..11), a deck of a
        // ground without a brush between them and a loose crate beside the hull. Four blocking stacks next to a building floor
        // belong to the building, not to a structure.
        FloorBounds fb{7, 0, 0, 48, 48};
        auto map = SemanticMap::create(std::span(&fb, 1), {2304}).value();
        const auto sand = map.internGroundKey("sand").value();
        const auto sea = map.internGroundKey("sea").value();
        const auto floorKey = map.internGroundKey("sandstone floor").value();
        std::vector<uint16_t> grounds(2304, 100);
        std::vector<PlacedStack> placed;
        for (int i = 0; i < 2304; ++i) {
            const int x = i % 48, y = i / 48;
            const bool deck = x >= 8 && x < 12 && (y == 11 || y == 12), hall = x >= 20 && x < 25 && y >= 15 && y < 20;
            if (deck) grounds[size_t(i)] = 900;
            if (x >= 8 && x < 12 && (y == 10 || y == 13)) placed.push_back({uint32_t(i), {500}, true});
            if (x == 19 && y >= 15 && y < 19) placed.push_back({uint32_t(i), {700}, true});
            const uint16_t key = deck ? noGroundKey : hall ? floorKey : x < 10 ? sea : sand;
            check(bool(map.set(x, y, 7, key, bit(Trait::HasGround))), "wreck cell");
        }
        placed.push_back({uint32_t(11 * 48 + 12), {600}, false});
        const auto mapping = proposeKeyMap(map.groundKeys(), defaultKeyRules());
        const auto learned = inspectLearningSample(map, mapping, {}, true, {}, {}, {}, placed, {}, grounds);
        const auto* wreck = learned && learned->structures.size() == 1 ? &learned->structures[0] : nullptr;
        check(wreck && wreck->width == 5 && wreck->height == 4 && wreck->ppm == 434 && wreck->cells.size() == 17, "wreck box and rate");
        check(wreck && wreck->cells[0] == StyleStructureCell{0, 0, 0, {500}} && wreck->cells[4] == StyleStructureCell{0, 1, 900, {}} &&
            wreck->cells[8] == StyleStructureCell{4, 1, 0, {600}}, "wreck hull, deck and crate");
        check(wreck && wreck->water.size() == 20 && wreck->water[0] == 1 && wreck->water[2] == 0 && wreck->water[5] == 0, "wreck water");
        check(learned && learned->wallStacks.empty(), "wreck stacks are no walls");
        auto profile = *a;
        if (wreck) profile.structures = learned->structures;
        check(wreck && parseProfile(*serializeProfile(profile)) == profile, "structures round-trip");
    }
    {
        // A 4 x 4 mountain on grass: 20 grass tiles touch it, 15 of them carry the optional border.
        FloorBounds fb{7, 0, 0, 32, 32};
        auto map = SemanticMap::create(std::span(&fb, 1), {1024}).value();
        const auto grass = map.internGroundKey("grass").value();
        const auto rock = map.internGroundKey("mountain").value();
        size_t flagged = 0;
        for (int i = 0; i < 1024; ++i) {
            const int x = i % 32, y = i / 32;
            const bool mountain = x >= 6 && x < 10 && y >= 6 && y < 10, ring = !mountain && x >= 5 && x < 11 && y >= 5 && y < 11;
            const bool outline = ring && flagged < 15;
            flagged += outline;
            check(bool(map.set(x, y, 7, mountain ? rock : grass, bit(Trait::HasGround) | (outline ? bit(Trait::OptionalBorder) : 0))), "outline cell");
        }
        const auto mapping = proposeKeyMap(map.groundKeys(), defaultKeyRules());
        const auto learned = inspectLearningSample(map, mapping, {}, true);
        check(learned && learned->rockOutlinePermille == 750, "mountain outline share");
        // No stairs by the mountain: its top cannot be climbed; all the grass is one walkable area.
        check(learned && learned->unreachedRockPermille == 1000 && learned->unreachedWalkPermille == 0, "mountain out of reach");
        auto profile = *a; // a valid fitted profile: serializing an invalid one returns an error
        profile.unreachedRockPermille = 1000;
        profile.unreachedWalkPermille = 12;
        const auto text = serializeProfile(profile);
        check(text && parseProfile(*text) == profile, "unreached round-trip");
    }
    check(rawDecorId(rawDecorName(2700, DecorKind::Tree)) == 2700 && classifyDoodadBrush(rawDecorName(1, DecorKind::Stone)) == DecorKind::Stone
        && !rawDecorId("raw:tree:0") && !rawDecorId("raw:tree:070") && !rawDecorId("raw:bush:65536") && !rawDecorId("raw:2700")
        && !rawDecorId("raw:rock:12a") && !rawDecorId("palm"), "RAW vegetation names");
    check(a->sources == std::vector<ProfileSource>{{"user-local-surface",1000}}, "generic user provenance");
    // I62: the learned bands are the profile's own (none for paths and buildings), never M06/M19's.
    check(!a->bands.empty() && candidateBandsFor(*a).data() == a->bands.data() &&
        std::ranges::none_of(a->bands, [](const CandidateBand& band) { return !learnableCandidateMetric(band.metric); }), "user profile ranks by its learned bands");
    {
        // I62: several samples. A second map of grass with a rock block and a pond, and a third of dirt.
        const auto other = [](int variant) {
            FloorBounds fb{7, 0, 0, 64, 64};
            auto map = SemanticMap::create(std::span(&fb, 1), {4096}).value();
            const auto grass = map.internGroundKey("grass").value();
            const auto rock = map.internGroundKey("mountain").value();
            const auto water = map.internGroundKey("sea").value();
            const auto dirt = map.internGroundKey("dirt").value();
            for (int i = 0; i < 4096; ++i) {
                const int x = i % 64, y = i / 64;
                const auto key = variant ? (x < 32 ? dirt : grass) : x >= 8 && x < 24 && y >= 8 && y < 24 ? rock : x >= 40 && x < 52 && y >= 40 && y < 52 ? water : grass;
                check(bool(map.set(x, y, 7, key, bit(Trait::HasGround))), "merge fixture cell");
            }
            return map;
        };
        const auto mapB = other(0), mapC = other(1);
        const auto mappingB = proposeKeyMap(mapB.groundKeys(), defaultKeyRules()), mappingC = proposeKeyMap(mapC.groundKeys(), defaultKeyRules());
        const auto sB = inspectLearningSample(mapB, mappingB, {}, true), sC = inspectLearningSample(mapC, mappingC, {}, true);
        check(sB && sC && sB->metrics[0] && sC->metrics[0], "samples measured");
        if (sB && sC) {
            const std::vector<std::vector<KeyMapEntry>> mappings {mapping, mappingB, mappingC};
            const auto merged = mergeLearningMappings(mappings);
            check(merged && std::ranges::is_sorted(*merged, {}, &KeyMapEntry::key), "mappings merged");
            auto conflict = mappingB;
            for (auto& entry : conflict) if (entry.key == "grass") entry.kind = StyleKind::Dirt;
            const std::vector<std::vector<KeyMapEntry>> conflicting {mapping, conflict};
            check(!mergeLearningMappings(conflicting), "conflicting classes refused");
            const auto one = mergeLearningSamples(std::span(&*sample, 1));
            check(one && one->metrics == sample->metrics && one->stats == sample->stats && one->sampleWeights.empty(), "one sample is itself");
            const std::vector<LearningSample> abc {*sample, *sB, *sC}, cab {*sC, *sample, *sB};
            const auto m1 = mergeLearningSamples(abc), m2 = mergeLearningSamples(cab);
            const auto p1 = m1 ? fitLearningSample(*m1, *merged, "10.98", "otb", "dat") : std::unexpected(std::string("no merge"));
            const auto p2 = m2 ? fitLearningSample(*m2, *merged, "10.98", "otb", "dat") : std::unexpected(std::string("no merge"));
            check(p1 && p2 && serializeProfile(*p1) == serializeProfile(*p2), "sample order does not change the profile");
            check(p1 && p1->sources.size() == 3 && p1->sources[0].alias == "sample-1" &&
                std::ranges::fold_left(p1->sources, 0u, [](uint32_t sum, const ProfileSource& s) { return sum + s.weightPermille; }) == 1000,
                "three weighted sources");
            check(p1 && m1 && m1->stats.kindTiles[size_t(StyleKind::Rock)] == sB->stats.kindTiles[size_t(StyleKind::Rock)] &&
                m1->present == sample->present + sB->present + sC->present, "counts summed");
            check(p1 && !p1->bands.empty() && parseProfile(*serializeProfile(*p1)) == *p1, "merged profile with bands round-trips");
            const std::vector<LearningSample> twice {*sB, *sB};
            const auto doubled = mergeLearningSamples(twice);
            const auto pTwice = doubled ? fitLearningSample(*doubled, mappingB, "10.98", "otb", "dat") : std::unexpected(std::string("no merge"));
            const auto pOnce = fitLearningSample(*sB, mappingB, "10.98", "otb", "dat");
            check(pTwice && pOnce && pTwice->target == pOnce->target && pTwice->tuning == pOnce->tuning && pTwice->bands == pOnce->bands,
                "two identical samples fit as one");
            const std::vector<LearningSample> nine(9, *sB);
            check(!mergeLearningSamples(nine) && !mergeLearningSamples(std::span<const LearningSample>()), "1 to 8 samples");
        }
    }
    check(checkProfile(*a,"wrong","otb","dat",7) == ProfileCompatibility::WrongClient, "client bound");
    check(checkProfile(*a,"10.98","wrong","dat",7) == ProfileCompatibility::WrongAssets, "assets bound");
    for (const char* name : {"", "..", "../x", "a/b", "a\\b", "CON", "com1", "a.profile", "a:", "a "})
        check(!validLearnedProfileName(name), "unsafe name refused");
    check(validLearnedProfileName("user-forest_01"), "portable name accepted");
    const std::filesystem::path dir(argc > 1 ? argv[1] : "scratchpad/style-learning-tests");
    std::filesystem::create_directories(dir);
    const auto unique = dir / "profile-store";
    std::filesystem::create_directories(unique);
    // Test directory belongs to this test, supplied explicitly under scratchpad by CTest.
    const auto forest = unique / "forest.profile";
    const auto newForest = unique / "new-forest.profile";
    std::error_code ec;
    std::filesystem::remove(forest, ec);
    std::filesystem::remove(newForest, ec);
    const auto readProfile = [](const std::filesystem::path& path) {
        std::ifstream in(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in), {});
    };
    auto replacement = *a;
    replacement.sources.front().alias = "learned-forest";
    const auto replacementText = serializeProfile(replacement);
    check(replacementText && replacementText != serializeProfile(*a), "different replacement text");
    check(bool(saveLearnedProfile(unique, "forest", *a)), "atomic publish");
    const auto text = readProfile(forest);
    check(text == *serializeProfile(*a), "initial profile bytes");
    check(!saveLearnedProfile(unique, "forest", replacement, false), "no overwrite without replace");
    check(readProfile(forest) == text, "conflict preserves previous bytes");
    check(bool(saveLearnedProfile(unique, "forest", replacement, true)), "replace existing profile");
    check(readProfile(forest) == replacementText, "replacement writes new bytes");
    check(bool(saveLearnedProfile(unique, "new-forest", replacement, true)), "replace creates missing profile");
    check(readProfile(newForest) == replacementText, "new profile has expected bytes");
    check(!saveLearnedProfile(unique, "../escape", *a, false), "no path escape");
    const auto offer = offerProfiles(std::vector<NamedProfileText>{{"forest", text}}, "10.98", "otb", "dat");
    check(offer.offered.size() == 1, "normal generator offer");
    const auto full = dir / "full-profile-store";
    std::filesystem::create_directories(full);
    const auto overflow = full / "overflow.profile";
    std::filesystem::remove(overflow, ec);
    for (int i = 0; i < 64; ++i) {
        const auto path = full / ("profile-" + std::to_string(i) + ".profile");
        check(std::filesystem::copy_file(newForest, path, std::filesystem::copy_options::overwrite_existing, ec),
            "seed profile at capacity");
    }
    check(bool(saveLearnedProfile(full, "profile-0", *a, true)), "replace at capacity");
    check(readProfile(full / "profile-0.profile") == text, "replacement at capacity writes new bytes");
    check(!saveLearnedProfile(full, "overflow", *a, false), "new profile refused at capacity");
    check(!saveLearnedProfile(full, "overflow", *a, true), "replace cannot create a 65th profile");
    check(!std::filesystem::exists(overflow), "no extra profile created");
    const auto blocker = dir / "file-not-folder"; { std::ofstream out(blocker); out << "block"; }
    check(!saveLearnedProfile(blocker, "forest", *a, false), "write failure visible");
    std::filesystem::create_directory(unique / ".learning-save.lock", ec);
    check(!saveLearnedProfile(unique, "locked", *a, true), "concurrent save conflict");
    std::filesystem::remove(unique / ".learning-save.lock", ec);
    auto history = StyleHistoryStore::open(dir / "history.json");
    check(bool(history), "I54 store opens");
    if (!failures) std::cout << "STYLE_LEARNING_PASS\n";
    return failures ? 1 : 0;
}
