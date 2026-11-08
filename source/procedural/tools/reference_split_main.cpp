// SPDX-License-Identifier: GPL-3.0-or-later
// Offline tool: manifest + one fingerprint per map -> family assignment, overlap table and
// leave-one-family-out folds as a deterministic JSON report. Reads only; no editor, no GUI.
//
// Usage: reference_split --manifest FILE --fingerprints DIR [--merge-permille N] --out FILE
// DIR holds <alias>.fp files written by the reference import probe.

#include "procedural/core/reference_split.h"

#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

using namespace rme::procedural;
namespace fs = std::filesystem;

namespace {

std::string readFile(const fs::path& path, bool& ok) {
	std::ifstream in(path, std::ios::binary);
	ok = static_cast<bool>(in);
	return ok ? std::string(std::istreambuf_iterator<char>(in), {}) : std::string {};
}

std::string quote(const std::string& text) { return '"' + text + '"'; } // aliases and families are plain tokens

} // namespace

int main(int argc, char** argv) {
	fs::path manifestPath, fingerprintDir, outPath;
	FamilyOptions options; // defaults fixed before any holdout result was seen
	uint32_t genericDivisor = 4; // a window found in at least 1/divisor of the maps is generic (0 = never)
	for (int i = 1; i + 1 < argc; i += 2) {
		const std::string key = argv[i];
		if (key == "--manifest") manifestPath = argv[i + 1];
		else if (key == "--fingerprints") fingerprintDir = argv[i + 1];
		else if (key == "--out") outPath = argv[i + 1];
		else if (key == "--merge-permille") options.mergePermille = static_cast<uint32_t>(std::strtoul(argv[i + 1], nullptr, 10));
		else if (key == "--min-shared") options.minSharedWindows = std::strtoull(argv[i + 1], nullptr, 10);
		else if (key == "--generic-divisor") genericDivisor = static_cast<uint32_t>(std::strtoul(argv[i + 1], nullptr, 10));
		else { std::cerr << "unknown option " << key << '\n'; return 2; }
	}
	if (manifestPath.empty() || fingerprintDir.empty() || outPath.empty() || options.mergePermille == 0 || options.mergePermille > 1000) {
		std::cerr << "usage: reference_split --manifest FILE --fingerprints DIR [--merge-permille 1..1000] --out FILE\n";
		return 2;
	}
	bool ok = false;
	const std::string manifestText = readFile(manifestPath, ok);
	if (!ok) { std::cerr << "cannot read manifest\n"; return 2; }
	const auto manifest = parseManifest(manifestText);
	if (!manifest) { std::cerr << "invalid manifest\n"; return 3; }

	std::vector<Fingerprint> prints;
	for (const auto& entry : manifest->entries) {
		const std::string text = readFile(fingerprintDir / (entry.alias + ".fp"), ok);
		if (!ok) { std::cerr << "missing fingerprint for " << entry.alias << '\n'; return 3; }
		auto parsed = parseFingerprint(text);
		if (!parsed) { std::cerr << "invalid fingerprint for " << entry.alias << '\n'; return 3; }
		prints.push_back(std::move(*parsed));
	}
	const uint32_t genericAtLeast = genericThreshold(prints.size(), genericDivisor);
	const std::vector<Fingerprint> specific = withoutGenericWindows(prints, genericAtLeast);
	const auto families = assignFamilies(*manifest, specific, options);
	const auto folds = makeFolds(*manifest, families, specific, options);

	const auto& e = manifest->entries;
	std::string out = "{\n  \"schema\": \"g4-split-report/1\",\n";
	out += std::format("  \"client\": {},\n  \"merge_permille\": {}, \"min_shared_windows\": {}, \"generic_at_least_maps\": {},\n",
		quote(e.empty() ? "" : e.front().client), options.mergePermille, options.minSharedWindows, genericAtLeast);
	out += std::format("  \"window\": {}, \"sample_modulo\": {}, \"min_present_percent\": {},\n",
		prints.front().options.window, prints.front().options.sampleModulo, prints.front().options.minPresentPercent);
	out += "  \"maps\": [\n";
	for (size_t i = 0; i < e.size(); ++i) {
		const auto& f = prints[i];
		out += std::format("    {{\"alias\": {}, \"role\": {}, \"declared_family\": {}, \"family\": {}, \"windows\": {}, \"sparse\": {}, \"candidates\": {}, \"uniform\": {}, \"kept\": {}, \"specific\": {}}}{}\n",
			quote(e[i].alias), quote(referenceRoleName(e[i].role)), quote(e[i].family), quote(families.family[i]),
			f.windows, f.sparseWindows, f.candidateWindows, f.uniformWindows, f.hashes.size(), specific[i].hashes.size(), i + 1 < e.size() ? "," : "");
	}
	out += "  ],\n  \"pairs_with_shared_specific_windows\": [\n";
	bool first = true;
	for (size_t a = 0; a < e.size(); ++a) for (size_t b = a + 1; b < e.size(); ++b) {
		const Containment c = compareFingerprints(specific[a], specific[b]);
		if (!c.comparable || c.shared == 0) continue;
		out += std::format("{}    {{\"a\": {}, \"b\": {}, \"shared\": {}, \"a_in_b_permille\": {}, \"b_in_a_permille\": {}}}", first ? "" : ",\n",
			quote(e[a].alias), quote(e[b].alias), c.shared, c.aInBPermille(), c.bInAPermille());
		first = false;
	}
	out += "\n  ],\n  \"not_comparable\": [";
	first = true;
	for (size_t i = 0; i < e.size(); ++i) {
		if (!specific[i].hashes.empty()) continue;
		out += std::format("{}{}", first ? "" : ", ", quote(e[i].alias));
		first = false;
	}
	out += "],\n  \"merged_by_overlap\": [";
	for (size_t i = 0; i < families.byOverlap.size(); ++i) {
		const auto& m = families.byOverlap[i];
		out += std::format("{}{{\"a\": {}, \"b\": {}, \"permille\": {}}}", i ? ", " : "", quote(e[m.a].alias), quote(e[m.b].alias), m.permille);
	}
	out += "],\n  \"folds\": ";
	if (!folds) {
		out += "null,\n  \"folds_error\": \"too_few_style_families\"\n}\n";
	} else {
		out += "[\n";
		auto list = [&](const std::vector<size_t>& group) {
			std::string s = "[";
			for (size_t i = 0; i < group.size(); ++i) s += std::format("{}{}", i ? ", " : "", quote(e[group[i]].alias));
			return s + "]";
		};
		for (size_t i = 0; i < folds->size(); ++i) {
			const Fold& f = (*folds)[i];
			out += std::format("    {{\"test_family\": {}, \"validation_family\": {}, \"test\": {}, \"validation\": {}, \"train\": {}, \"max_leak_permille\": {}}}{}\n",
				quote(f.testFamily), quote(f.validationFamily), list(f.test), list(f.validation), list(f.train), f.maxLeakPermille,
				i + 1 < folds->size() ? "," : "");
		}
		out += "  ]\n}\n";
	}
	std::ofstream file(outPath, std::ios::binary);
	file << out;
	return file ? 0 : 4;
}
