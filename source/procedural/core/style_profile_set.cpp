// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/style_profile_set.h"

namespace rme::procedural {

ProfileOffer offerProfiles(std::span<const NamedProfileText> files, std::string_view client,
	std::string_view otbSha256, std::string_view datSha256) {
	ProfileOffer offer;
	for (const NamedProfileText& file : files) {
		auto parsed = parseProfile(file.text);
		if (!parsed || parsed->level != StyleLevel::Surface || !parsed->tuning) { ++offer.notUsable; continue; }
		// Surface floors are z 0..7; the generator works on one floor of the region the user picks, so a profile
		// is offered if it covers at least floor 7 (the floor its statistics were learned on).
		switch (checkProfile(*parsed, client, otbSha256, datSha256, 7)) {
			case ProfileCompatibility::WrongClient: ++offer.otherClient; continue;
			case ProfileCompatibility::WrongAssets: ++offer.otherAssets; continue;
			case ProfileCompatibility::WrongLevel: ++offer.notUsable; continue;
			case ProfileCompatibility::Compatible: break;
		}
		if (parsed->decorTuning && !valid(*parsed->decorTuning)) { ++offer.notUsable; continue; }
		// The generator's own range check, on a small dummy region.
		const SurfaceConfig probe {{0, 0, 7, 16, 16}, 1, *parsed->tuning};
		const auto checked = validateSurface(probe, MapExtent {65001, 65001, 16}, SurfaceBudget {1u << 20, 1u << 24, 1u << 28});
		if (!checked && checked.error() == SurfaceError::InvalidParameters) { ++offer.notUsable; continue; }
		offer.offered.push_back({file.name, std::move(*parsed)});
	}
	return offer;
}

} // namespace rme::procedural
