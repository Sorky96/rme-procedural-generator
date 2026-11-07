// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/reference_split.h"

#include <algorithm>
#include <map>
#include <numeric>
#include <set>

namespace rme::procedural {
namespace {

std::vector<std::string_view> splitTabs(std::string_view line) {
	std::vector<std::string_view> fields;
	while (true) {
		const size_t tab = line.find('\t');
		fields.push_back(line.substr(0, tab));
		if (tab == std::string_view::npos) break;
		line.remove_prefix(tab + 1);
	}
	return fields;
}

struct UnionFind {
	std::vector<size_t> parent;
	explicit UnionFind(size_t n) : parent(n) { std::iota(parent.begin(), parent.end(), size_t {0}); }
	size_t find(size_t x) { while (parent[x] != x) x = parent[x] = parent[parent[x]]; return x; }
	void unite(size_t a, size_t b) { parent[find(a)] = find(b); }
};

} // namespace

const char* referenceRoleName(ReferenceRole role) noexcept {
	switch (role) {
		case ReferenceRole::Style: return "style";
		case ReferenceRole::Sample: return "sample";
		case ReferenceRole::Shape: return "shape";
	}
	return "?";
}

std::expected<Manifest, ManifestError> parseManifest(std::string_view text) {
	Manifest manifest;
	bool header = false;
	std::set<std::string> aliases;
	while (!text.empty()) {
		const size_t end = text.find('\n');
		std::string_view line = text.substr(0, end);
		text = end == std::string_view::npos ? std::string_view {} : text.substr(end + 1);
		if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
		if (line.empty() || line.front() == '#') continue;
		if (!header) {
			if (line != manifestSchema) return std::unexpected(ManifestError::BadFormat);
			header = true;
			continue;
		}
		const auto fields = splitTabs(line);
		if (fields.size() != 5) return std::unexpected(ManifestError::BadFormat);
		for (const auto field : fields) if (field.empty()) return std::unexpected(ManifestError::EmptyField);
		ManifestEntry entry;
		entry.alias = fields[0];
		entry.sha256 = fields[1];
		entry.client = fields[2];
		if (fields[3] == "style") entry.role = ReferenceRole::Style;
		else if (fields[3] == "sample") entry.role = ReferenceRole::Sample;
		else if (fields[3] == "shape") entry.role = ReferenceRole::Shape;
		else return std::unexpected(ManifestError::UnknownRole);
		entry.family = fields[4];
		if (!aliases.insert(entry.alias).second) return std::unexpected(ManifestError::DuplicateAlias);
		if (!manifest.entries.empty() && manifest.entries.front().client != entry.client) return std::unexpected(ManifestError::MixedClients);
		manifest.entries.push_back(std::move(entry));
	}
	if (!header) return std::unexpected(ManifestError::BadFormat);
	return manifest;
}

std::string renderManifest(const Manifest& manifest) {
	std::string out(manifestSchema);
	out += '\n';
	for (const auto& e : manifest.entries) {
		out += e.alias + '\t' + e.sha256 + '\t' + e.client + '\t' + referenceRoleName(e.role) + '\t' + e.family + '\n';
	}
	return out;
}

FamilyAssignment assignFamilies(const Manifest& manifest, std::span<const Fingerprint> fingerprints, const FamilyOptions& options) {
	const size_t n = manifest.entries.size();
	UnionFind sets(n);
	std::map<std::string, size_t> firstOfDeclared;
	for (size_t i = 0; i < n; ++i) {
		const auto [it, inserted] = firstOfDeclared.emplace(manifest.entries[i].family, i);
		if (!inserted) sets.unite(i, it->second);
	}
	FamilyAssignment result;
	for (size_t a = 0; a < n && a < fingerprints.size(); ++a) {
		for (size_t b = a + 1; b < n && b < fingerprints.size(); ++b) {
			if (sets.find(a) == sets.find(b)) continue;
			const Containment c = compareFingerprints(fingerprints[a], fingerprints[b]);
			if (c.comparable && c.shared >= options.minSharedWindows && c.maxPermille() >= options.mergePermille) {
				sets.unite(a, b);
				result.byOverlap.push_back({a, b, c.maxPermille()});
			}
		}
	}
	std::map<size_t, std::string> name;
	for (size_t i = 0; i < n; ++i) {
		auto [it, inserted] = name.emplace(sets.find(i), manifest.entries[i].family);
		if (!inserted && manifest.entries[i].family < it->second) it->second = manifest.entries[i].family;
	}
	result.family.reserve(n);
	for (size_t i = 0; i < n; ++i) result.family.push_back(name[sets.find(i)]);
	return result;
}

std::expected<std::vector<Fold>, SplitError> makeFolds(
	const Manifest& manifest, const FamilyAssignment& families, std::span<const Fingerprint> fingerprints,
	const FamilyOptions& options) {
	if (families.family.size() != manifest.entries.size() || fingerprints.size() != manifest.entries.size()) {
		return std::unexpected(SplitError::InvalidInput);
	}
	std::map<std::string, std::vector<size_t>> styleMembers; // sorted by family id
	for (size_t i = 0; i < manifest.entries.size(); ++i) {
		if (manifest.entries[i].role == ReferenceRole::Style) styleMembers[families.family[i]].push_back(i);
	}
	if (styleMembers.size() < 3) return std::unexpected(SplitError::TooFewFamilies);

	std::vector<std::string> order;
	for (const auto& [family, members] : styleMembers) order.push_back(family);
	std::vector<Fold> folds;
	for (size_t f = 0; f < order.size(); ++f) {
		Fold fold;
		fold.testFamily = order[f];
		fold.validationFamily = order[(f + 1) % order.size()];
		fold.test = styleMembers[fold.testFamily];
		fold.validation = styleMembers[fold.validationFamily];
		for (const auto& family : order) {
			if (family != fold.testFamily && family != fold.validationFamily) {
				fold.train.insert(fold.train.end(), styleMembers[family].begin(), styleMembers[family].end());
			}
		}
		for (const size_t t : fold.test) {
			for (const auto* group : {&fold.validation, &fold.train}) {
				for (const size_t o : *group) {
					const Containment c = compareFingerprints(fingerprints[t], fingerprints[o]);
					if (c.shared >= options.minSharedWindows) fold.maxLeakPermille = std::max(fold.maxLeakPermille, c.maxPermille());
				}
			}
		}
		folds.push_back(std::move(fold));
	}
	return folds;
}

} // namespace rme::procedural
