// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/reference_split.h"
#include "procedural/core/similarity.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

using namespace rme::procedural;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
	if (!ok) { ++failures; std::cerr << "FAIL: " << what << '\n'; }
}

uint64_t splitmix(uint64_t& state) {
	uint64_t z = (state += 0x9E3779B97F4A7C15ull);
	z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
	z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
	return z ^ (z >> 31);
}

// A square map of random ground drawn from `names`; `reverseKeys` interns them in the opposite
// order so the key indices differ while the names stay the same.
struct Cells { int w, h; std::vector<int> value; }; // index into names, -1 = no tile

Cells randomCells(int w, int h, uint64_t seed, int names) {
	Cells c {w, h, std::vector<int>(static_cast<size_t>(w) * h)};
	uint64_t state = seed;
	for (auto& v : c.value) v = static_cast<int>(splitmix(state) % static_cast<uint64_t>(names));
	return c;
}

void paste(Cells& dst, const Cells& src, int sx, int sy, int bw, int bh, int dx, int dy) {
	for (int y = 0; y < bh; ++y) for (int x = 0; x < bw; ++x) {
		dst.value[static_cast<size_t>(dy + y) * dst.w + dx + x] = src.value[static_cast<size_t>(sy + y) * src.w + sx + x];
	}
}

SemanticMap build(const Cells& c, int z, bool reverseKeys, int names = 5) {
	auto map = SemanticMap::create(std::vector<FloorBounds> {{z, 0, 0, c.w, c.h}}, {10'000'000}).value();
	std::vector<uint16_t> key(static_cast<size_t>(names));
	for (int i = 0; i < names; ++i) {
		const int n = reverseKeys ? names - 1 - i : i;
		key[static_cast<size_t>(n)] = map.internGroundKey("ground-" + std::to_string(n)).value();
	}
	for (int y = 0; y < c.h; ++y) for (int x = 0; x < c.w; ++x) {
		const int v = c.value[static_cast<size_t>(y) * c.w + x];
		if (v >= 0) check(map.set(x, y, z, key[static_cast<size_t>(v)], bit(Trait::HasGround)).has_value(), "set");
	}
	return map;
}

Fingerprint fp(const SemanticMap& map, FingerprintOptions o = {}) {
	const auto r = computeFingerprint(map, o, {});
	check(r.has_value(), "fingerprint succeeds");
	return r ? *r : Fingerprint {};
}

void testIdenticalAndIndependent() {
	FingerprintOptions all {16, 1, 50};
	const Cells a = randomCells(80, 70, 1, 5);
	const Fingerprint fa = fp(build(a, 7, false), all);
	const Fingerprint fa2 = fp(build(a, 7, false), all);
	check(fa.hashes == fa2.hashes && fa.windows == fa2.windows, "same input gives same fingerprint");
	check(fa.windows == static_cast<uint64_t>(80 - 15) * (70 - 15), "all complete windows examined");
	check(fa.hashes.size() == fa.windows, "random content has distinct windows");
	check(std::is_sorted(fa.hashes.begin(), fa.hashes.end()), "hashes sorted");
	const Containment self = compareFingerprints(fa, fa);
	check(self.comparable && self.aInBPermille() == 1000 && self.bInAPermille() == 1000, "map contains itself");

	const Fingerprint fb = fp(build(randomCells(80, 70, 2, 5), 7, false), all);
	const Containment indep = compareFingerprints(fa, fb);
	check(indep.comparable && indep.shared == 0, "independent maps share no window");
}

void testShiftedCopyAcrossKeyOrderAndFloor() {
	FingerprintOptions all {16, 1, 50};
	const Cells a = randomCells(80, 70, 11, 5);
	Cells b = randomCells(90, 60, 12, 5);
	paste(b, a, 5, 7, 30, 30, 40, 3); // 30x30 block, moved and placed on another floor with another key table
	const Fingerprint fa = fp(build(a, 7, false), all);
	const Fingerprint fb = fp(build(b, 9, true), all);
	const Containment c = compareFingerprints(fa, fb);
	check(c.shared == static_cast<uint64_t>(30 - 15) * (30 - 15), "moved copy found: every full window of the block");
	check(c.bInAPermille() == 1000 * 225 / fb.hashes.size(), "permille of the copying map");

	// With the default content sample the same copy still shows up, on both sides identically.
	const Containment sampled = compareFingerprints(fp(build(a, 7, false)), fp(build(b, 9, true)));
	check(sampled.shared > 0 && sampled.shared <= 225, "sampled copy detected");
}

void testUniformAndSparse() {
	FingerprintOptions all {16, 1, 50};
	Cells flat {64, 64, std::vector<int>(64 * 64, 0)};
	const Fingerprint f = fp(build(flat, 7, false), all);
	check(f.hashes.empty() && f.uniformWindows == f.candidateWindows && f.uniformWindows == static_cast<uint64_t>(49) * 49, "uniform windows are dropped");
	check(!compareFingerprints(f, f).comparable, "empty fingerprints are not comparable");

	Cells half = randomCells(64, 64, 5, 5);
	for (int y = 0; y < 64; ++y) for (int x = 0; x < 64; ++x) if (x % 2 == 0) half.value[static_cast<size_t>(y) * 64 + x] = -1;
	const Fingerprint s = fp(build(half, 7, false), {16, 1, 60});
	check(s.sparseWindows == s.windows && s.hashes.empty(), "windows below the present share are sparse");
	const Fingerprint ok = fp(build(half, 7, false), {16, 1, 50});
	check(ok.sparseWindows < ok.windows, "half-present windows pass at 50 percent");

	auto tiny = SemanticMap::create(std::vector<FloorBounds> {{7, 0, 0, 10, 10}}, {1000}).value();
	check(fp(tiny).windows == 0, "floor smaller than the window yields no windows");
}

void testOptionsCancelAndFormat() {
	const SemanticMap map = build(randomCells(40, 40, 3, 5), 7, false);
	check(computeFingerprint(map, {1, 1, 50}, {}).error() == SimilarityError::InvalidOptions, "window 1 refused");
	check(computeFingerprint(map, {16, 0, 50}, {}).error() == SimilarityError::InvalidOptions, "modulo 0 refused");
	check(computeFingerprint(map, {16, 1, 101}, {}).error() == SimilarityError::InvalidOptions, "percent over 100 refused");
	std::stop_source source;
	source.request_stop();
	check(computeFingerprint(map, {}, source.get_token()).error() == SimilarityError::Cancelled, "cancelled before work");

	const Fingerprint a = fp(map, {16, 1, 50}), b = fp(map, {8, 1, 50});
	check(!compareFingerprints(a, b).comparable, "different window sizes are not comparable");

	const std::string text = serializeFingerprint(a);
	const auto back = parseFingerprint(text);
	check(back && back->hashes == a.hashes && back->options == a.options && back->windows == a.windows, "round trip");
	check(serializeFingerprint(*back) == text, "serialization is stable");
	check(!parseFingerprint("nonsense\n"), "bad header refused");
	check(!parseFingerprint(text.substr(0, text.size() - 17)), "missing hash line refused (count mismatch)");
	std::string swapped = text;
	const size_t firstHash = swapped.find('\n', swapped.find('\n') + 1) + 1;
	std::swap_ranges(swapped.begin() + firstHash, swapped.begin() + firstHash + 16, swapped.begin() + firstHash + 17);
	check(!parseFingerprint(swapped), "unsorted hashes refused");
}

std::string manifestText(const std::vector<std::vector<std::string>>& rows) {
	std::string text = "g4-manifest/1\n# comment\n";
	for (const auto& r : rows) text += r[0] + "\t" + r[1] + "\t10.98\t" + r[2] + "\t" + r[3] + "\n";
	return text;
}

void testManifest() {
	const auto m = parseManifest(manifestText({{"M01", "aa", "style", "f1"}, {"M02", "bb", "sample", "f2"}, {"M07", "cc", "shape", "shape"}}));
	check(m && m->entries.size() == 3 && m->entries[1].role == ReferenceRole::Sample, "manifest parses");
	check(m && parseManifest(renderManifest(*m)) && renderManifest(*parseManifest(renderManifest(*m))) == renderManifest(*m), "manifest round trip");
	check(parseManifest(manifestText({{"M01", "aa", "style", "f"}, {"M01", "bb", "style", "g"}})).error() == ManifestError::DuplicateAlias, "duplicate alias");
	check(parseManifest(manifestText({{"M01", "aa", "oops", "f"}})).error() == ManifestError::UnknownRole, "unknown role");
	check(parseManifest("M01\taa\t10.98\tstyle\tf\n").error() == ManifestError::BadFormat, "missing header");
	check(parseManifest("g4-manifest/1\nM01\taa\t10.98\tstyle\n").error() == ManifestError::BadFormat, "wrong field count");
	check(parseManifest("g4-manifest/1\nM01\t\t10.98\tstyle\tf\n").error() == ManifestError::EmptyField, "empty field");
	check(parseManifest("g4-manifest/1\nM01\taa\t10.98\tstyle\tf\nM02\tbb\tother\tstyle\tg\n").error() == ManifestError::MixedClients, "mixed clients");
}

struct Corpus {
	Manifest manifest;
	std::vector<Fingerprint> prints;
};

// Six independent style maps (S0..S5), one of which (S4) also holds a copy from S1, plus a
// sample map that holds a copy from S2 and two shape maps that are the same map.
Corpus makeCorpus() {
	FingerprintOptions o {16, 1, 50};
	std::vector<Cells> cells;
	for (uint64_t i = 0; i < 6; ++i) cells.push_back(randomCells(70, 70, 100 + i, 6));
	paste(cells[4], cells[1], 3, 3, 40, 40, 20, 20);
	Cells sample = randomCells(60, 60, 200, 6);
	paste(sample, cells[2], 0, 0, 40, 40, 10, 10);
	const Cells shape = randomCells(60, 60, 300, 6);

	Corpus c;
	auto add = [&](const char* alias, ReferenceRole role, const char* family, const Cells& cell) {
		c.manifest.entries.push_back({alias, "00", "10.98", role, family});
		c.prints.push_back(fp(build(cell, 7, false, 6), o));
	};
	for (int i = 0; i < 6; ++i) add(("S" + std::to_string(i)).c_str(), ReferenceRole::Style, ("fam" + std::to_string(i)).c_str(), cells[static_cast<size_t>(i)]);
	add("X1", ReferenceRole::Sample, "samples", sample);
	add("H1", ReferenceRole::Shape, "shape", shape);
	add("H2", ReferenceRole::Shape, "shape", shape);
	return c;
}

void testFamiliesAndFolds() {
	const Corpus c = makeCorpus();
	const auto families = assignFamilies(c.manifest, c.prints, {});
	check(families.family[1] == families.family[4], "overlapping style maps share a family");
	check(families.family[1] == "fam1", "family id is the smallest declared name");
	check(families.family[2] == families.family[6], "sample overlapping a style map joins its family");
	check(families.family[7] == families.family[8] && families.family[7] == "shape", "declared family kept");
	check(families.family[0] != families.family[3] && families.family[0] != families.family[5], "independent maps stay apart");
	check(families.byOverlap.size() == 2, "two overlap merges recorded");

	const auto folds = makeFolds(c.manifest, families, c.prints, {});
	check(folds.has_value(), "folds made");
	if (!folds) return;
	check(folds->size() == 5, "one fold per style family: fam0, fam1(+S4), fam2, fam3, fam5");
	for (const Fold& fold : *folds) {
		std::vector<size_t> all;
		for (const auto* group : {&fold.test, &fold.validation, &fold.train}) all.insert(all.end(), group->begin(), group->end());
		std::sort(all.begin(), all.end());
		check(std::adjacent_find(all.begin(), all.end()) == all.end(), "no map in two partitions");
		check(all.size() == 6, "all six style maps used exactly once");
		check(std::none_of(all.begin(), all.end(), [&](size_t i) { return c.manifest.entries[i].role != ReferenceRole::Style; }), "samples and shapes never in a fold");
		check(!fold.test.empty() && !fold.validation.empty() && !fold.train.empty(), "all partitions non-empty");
		check(fold.maxLeakPermille < 100, "no leakage between test and the rest");
		check(fold.testFamily != fold.validationFamily, "validation is another family");
	}
	// Family of S1 and S4 stays together whichever role it plays.
	for (const Fold& fold : *folds) {
		auto in = [&](const std::vector<size_t>& g, size_t i) { return std::find(g.begin(), g.end(), i) != g.end(); };
		for (const auto* g : {&fold.test, &fold.validation, &fold.train}) check(in(*g, 1) == in(*g, 4), "family stays in one partition");
	}

	// Order of the entries does not change which families form folds.
	Corpus reversed = c;
	std::reverse(reversed.manifest.entries.begin(), reversed.manifest.entries.end());
	std::reverse(reversed.prints.begin(), reversed.prints.end());
	const auto folds2 = makeFolds(reversed.manifest, assignFamilies(reversed.manifest, reversed.prints, {}), reversed.prints, {});
	check(folds2 && folds2->size() == folds->size(), "same fold count after reordering");
	if (folds2) for (size_t i = 0; i < folds->size(); ++i) {
		check((*folds)[i].testFamily == (*folds2)[i].testFamily && (*folds)[i].validationFamily == (*folds2)[i].validationFamily, "fold families independent of entry order");
	}

	Manifest few = c.manifest;
	few.entries.resize(2);
	const auto fewFamilies = assignFamilies(few, std::span<const Fingerprint>(c.prints).first(2), {});
	check(makeFolds(few, fewFamilies, std::span<const Fingerprint>(c.prints).first(2), {}).error() == SplitError::TooFewFamilies, "fewer than three families refused");
	check(makeFolds(c.manifest, families, std::span<const Fingerprint>(c.prints).first(3), {}).error() == SplitError::InvalidInput, "mismatched inputs refused");

	// A threshold above any overlap merges nothing; one near zero merges the overlapping pairs only.
	check(assignFamilies(c.manifest, c.prints, {1001, 16}).byOverlap.empty(), "threshold above 1000 merges nothing");
}

void testGenericWindowsAndMinimumShared() {
	FingerprintOptions all {16, 1, 50};
	// Three maps share one 24x24 block (generic: found in all three); only A and B also share a second block.
	Cells a = randomCells(70, 70, 41, 6), b = randomCells(70, 70, 42, 6), c = randomCells(70, 70, 43, 6);
	paste(b, a, 0, 0, 24, 24, 30, 30);
	paste(c, a, 0, 0, 24, 24, 5, 5);
	paste(b, a, 40, 40, 28, 28, 2, 2);
	std::vector<Fingerprint> prints {fp(build(a, 7, false, 6), all), fp(build(b, 7, false, 6), all), fp(build(c, 7, false, 6), all)};
	const auto specific = withoutGenericWindows(prints, 3);
	const uint64_t blockWindows = (24 - 15) * (24 - 15);
	check(compareFingerprints(prints[0], prints[2]).shared == blockWindows, "the shared block is found before the filter");
	check(compareFingerprints(specific[0], specific[2]).shared == 0, "windows found in all three maps are dropped");
	check(compareFingerprints(specific[0], specific[1]).shared == (28 - 15) * (28 - 15), "pair-specific windows survive");
	check(specific[0].hashes.size() == prints[0].hashes.size() - blockWindows, "only generic windows removed from A");
	check(withoutGenericWindows(prints, 0)[0].hashes == prints[0].hashes && withoutGenericWindows(prints, 1)[1].hashes == prints[1].hashes, "threshold below 2 removes nothing");

	Manifest m;
	for (const char* alias : {"A", "B", "C"}) m.entries.push_back({alias, "00", "10.98", ReferenceRole::Style, alias});
	check(assignFamilies(m, specific, {10, 169}).byOverlap.size() == 1, "A and B merge with 169 shared windows (13x13)");
	check(assignFamilies(m, specific, {10, 170}).byOverlap.empty(), "one window more than shared: no merge");
	check(assignFamilies(m, prints, {10, 16}).byOverlap.size() == 2, "without the filter the generic block also joins C");
}

} // namespace

int main() {
	testIdenticalAndIndependent();
	testShiftedCopyAcrossKeyOrderAndFloor();
	testUniformAndSparse();
	testOptionsCancelAndFormat();
	testManifest();
	testFamiliesAndFolds();
	testGenericWindowsAndMinimumShared();
	if (failures == 0) std::cout << "OK\n";
	return failures == 0 ? 0 : 1;
}
