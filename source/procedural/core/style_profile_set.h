// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/core/style_profile.h"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rme::procedural {

// Which of a set of profile files may be offered for the loaded client. Pure: the adapter reads the
// files and hashes the client's assets, this decides. A profile is offered only if it is a valid
// surface profile for exactly this client and these assets and carries fitted tuning.

struct NamedProfileText {
	std::string name;   // shown to the user, normally the file stem
	std::string text;
};

struct OfferedProfile {
	std::string name;
	StyleProfile profile;
};

struct ProfileOffer {
	std::vector<OfferedProfile> offered;
	size_t otherClient = 0;      // learned for another client
	size_t otherAssets = 0;      // same client name, other items.otb or Tibia.dat
	size_t notUsable = 0;        // unreadable, wrong schema, underground, or without fitted tuning
};

// Order of `offered` follows `files`. A tuning whose values the generator would refuse counts as notUsable.
[[nodiscard]] ProfileOffer offerProfiles(std::span<const NamedProfileText> files, std::string_view client,
	std::string_view otbSha256, std::string_view datSha256);

} // namespace rme::procedural
