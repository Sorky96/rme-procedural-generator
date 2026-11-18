// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/core/style_profile_set.h"
#include "procedural/core/style_history.h"

#include <filesystem>
#include <optional>
#include <string>

namespace rme::procedural {

// Style profiles on disk (G4): files named *.profile in one folder, written by the offline learning tools from
// private reference maps. Source profiles are read-only; local feedback/version writes use
// StyleHistoryStore, independently of the map and Undo.

struct ClientAssetIds {
	std::string client;       // loaded client id, e.g. "10.98"
	std::string otbSha256;    // lowercase hex of the modular data's items.otb
	std::string datSha256;    // lowercase hex of the client's metadata file (Tibia.dat)
};

// GUI thread. Hashes the two files; nullopt when no client is loaded, the client has no items.otb, or a file
// cannot be read. Cost: one read of about 3 MiB.
[[nodiscard]] std::optional<ClientAssetIds> loadedClientAssetIds();

// <local data directory>/procedural/styles. The folder is not created.
[[nodiscard]] std::filesystem::path styleProfileDirectory();

// Add compatible local snapshots without modifying the read-only source profiles.
void offerStyleHistory(ProfileOffer& offer, const StyleHistory& history, const ClientAssetIds& ids);

struct StyleProfileFolder {
	std::filesystem::path path;
	bool exists = false;
	size_t unreadable = 0;      // files over the size limit or that could not be read
	ProfileOffer offer;
};

// Reads at most maxFiles files of at most maxFileBytes each, in name order, and offers the compatible ones.
[[nodiscard]] StyleProfileFolder scanStyleProfiles(const std::filesystem::path& folder, const ClientAssetIds& ids,
	size_t maxFiles = 64, size_t maxFileBytes = 1u << 20);

} // namespace rme::procedural
