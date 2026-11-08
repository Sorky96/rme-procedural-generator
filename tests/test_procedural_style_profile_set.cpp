// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/decoration.h"
#include "procedural/core/style_profile_set.h"

#include <iostream>

using namespace rme::procedural;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
	if (!ok) { ++failures; std::cerr << "FAIL: " << what << '\n'; }
}

StyleProfile make() {
	StyleProfile p;
	p.client = "10.98";
	p.otbSha256 = "aa";
	p.datSha256 = "bb";
	p.zMin = 0;
	p.zMax = 7;
	p.tuning = SurfaceTuning {};
	p.decorTuning = DecorTuning {};
	return p;
}

NamedProfileText named(const char* name, const StyleProfile& p) { return {name, serializeProfile(p).value()}; }

void testOffer() {
	StyleProfile good = make();
	good.tuning->lakePermille = 200;
	StyleProfile otherClient = make();
	otherClient.client = "1098-BlackTek";
	StyleProfile otherOtb = make();
	otherOtb.otbSha256 = "cc";
	StyleProfile otherDat = make();
	otherDat.datSha256 = "cc";
	StyleProfile underground = make();
	underground.level = StyleLevel::Underground;
	underground.zMin = 8;
	underground.zMax = 15;
	StyleProfile noTuning = make();
	noTuning.tuning.reset();
	StyleProfile badTuning = make();
	badTuning.tuning->lakePermille = 900; // the generator allows 0..500
	StyleProfile badDecor = make();
	badDecor.decorTuning->bushPermille = 1001;
	StyleProfile noDecor = make();
	noDecor.decorTuning.reset();

	const std::vector<NamedProfileText> files {
		named("good", good), named("other-client", otherClient), named("other-otb", otherOtb), named("other-dat", otherDat),
		named("underground", underground), named("no-tuning", noTuning), named("bad-tuning", badTuning), named("bad-decor", badDecor),
		{"garbage", "not a profile"}, {"empty", ""}, named("no-decor", noDecor), named("good-too", make()),
	};
	const ProfileOffer offer = offerProfiles(files, "10.98", "aa", "bb");
	check(offer.offered.size() == 3 && offer.offered[0].name == "good" && offer.offered[1].name == "no-decor" && offer.offered[2].name == "good-too", "compatible profiles, in file order");
	check(offer.offered.size() == 3 && offer.offered[0].profile.tuning->lakePermille == 200, "the parsed tuning is carried");
	check(offer.otherClient == 1, "one profile for another client");
	check(offer.otherAssets == 2, "two profiles for other items.otb or Tibia.dat");
	check(offer.notUsable == 6, "underground, no tuning, bad tuning, bad decor, garbage and empty are not usable");
	check(offer.offered.size() + offer.otherClient + offer.otherAssets + offer.notUsable == files.size(), "every file is accounted for");

	const ProfileOffer none = offerProfiles({}, "10.98", "aa", "bb");
	check(none.offered.empty() && none.otherClient == 0 && none.notUsable == 0, "no files, nothing offered");
	const ProfileOffer wrong = offerProfiles(files, "1098-BlackTek", "aa", "bb");
	check(wrong.offered.size() == 1 && wrong.offered[0].name == "other-client", "only the profile of that client is offered for it");
	check(offerProfiles(files, "10.98", "", "").offered.empty(), "no asset hashes: nothing matches");
}

void testConfig() {
	GenerationConfig a, b;
	check(a == b && a.decor == DecorTuning {}, "default configs are equal, decor defaults to the built-in rules");
	b.decor.bushPermille = 41;
	check(!(a == b), "a different decor tuning is a different config");
	check(valid(DecorTuning {}) && !valid(DecorTuning {maxTreeScalePermille + 1, 0, 40, 60, 30, 80}) && !valid(DecorTuning {1000, 1001, 40, 60, 30, 80}), "range check");
}

} // namespace

int main() {
	testOffer();
	testConfig();
	if (failures == 0) std::cout << "OK\n";
	return failures == 0 ? 0 : 1;
}
