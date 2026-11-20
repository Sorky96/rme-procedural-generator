// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/style_learning.h"
#include "procedural/core/style_profile_set.h"
#include "procedural/core/style_history.h"
#include "procedural/core/candidate_series.h"
#include <fstream>
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
    check(a->palette[size_t(StyleKind::Shore)] == a->palette[size_t(StyleKind::Grass)], "shoreless sample rings water with its grass");
    {
        // Every grass tile holds a palm: the learned profile keeps the palm brush and a vegetation fit.
        FloorBounds fb{7, 0, 0, 64, 64};
        auto forest = SemanticMap::create(std::span(&fb, 1), {4096}).value();
        const auto grass = forest.internGroundKey("grass").value();
        const auto jungle = forest.internGroundKey("jungle grass").value();
        const auto swamp = forest.internGroundKey("swamp").value();
        for (int i = 0; i < 4096; ++i) // a quarter jungle grass, an eighth swamp
            check(bool(forest.set(i % 64, i / 64, 7, i % 8 == 1 ? swamp : i % 4 ? grass : jungle, bit(Trait::HasGround) | bit(Trait::TreeDoodad))), "forest cell");
        auto forestMapping = proposeKeyMap(forest.groundKeys(), defaultKeyRules());
        for (auto& entry : forestMapping) if (entry.key == "swamp") entry.kind = StyleKind::Grass; // as the dialog proposes
        const std::vector<KeyTiles> palms{{"mangrove tree (on water)", 9000}, {"jungle palm", 1300}, {"oak tree", 1200},
            {"pine tree", 1100}, {"birch tree", 1000}, {"fir tree", 900}};
        const auto learned = inspectLearningSample(forest, forestMapping, palms, true);
        check(learned && learned->decorPalette[0] == "jungle palm", "tree brush learned");
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
    check(a->sources == std::vector<ProfileSource>{{"user-local-surface",1000}}, "generic user provenance");
    check(candidateBandsFor(*a).empty(), "user profile has no M06/M19 bands");
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
    std::error_code ec; std::filesystem::remove(unique / "forest.profile", ec);
    check(bool(saveLearnedProfile(unique, "forest", *a)), "atomic publish");
    check(!saveLearnedProfile(unique, "forest", *b), "no overwrite");
    check(!saveLearnedProfile(unique, "../escape", *a), "no path escape");
    std::ifstream in(unique / "forest.profile", std::ios::binary);
    const std::string text((std::istreambuf_iterator<char>(in)), {});
    check(text == *serializeProfile(*a), "conflict preserves previous bytes");
    const auto offer = offerProfiles(std::vector<NamedProfileText>{{"forest", text}}, "10.98", "otb", "dat");
    check(offer.offered.size() == 1, "normal generator offer");
    const auto blocker = dir / "file-not-folder"; { std::ofstream out(blocker); out << "block"; }
    check(!saveLearnedProfile(blocker, "forest", *a), "write failure visible");
    std::filesystem::create_directory(unique / ".learning-save.lock", ec);
    check(!saveLearnedProfile(unique, "locked", *a), "concurrent save conflict");
    std::filesystem::remove(unique / ".learning-save.lock", ec);
    auto history = StyleHistoryStore::open(dir / "history.json");
    check(bool(history), "I54 store opens");
    if (!failures) std::cout << "STYLE_LEARNING_PASS\n";
    return failures ? 1 : 0;
}
