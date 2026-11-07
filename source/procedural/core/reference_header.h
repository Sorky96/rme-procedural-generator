// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace rme::procedural {

// Streaming SHA-256 (FIPS 180-4). Used to prove that importing a reference map
// does not change the input file, and to identify references in manifests.
class Sha256 {
public:
	Sha256() noexcept;
	void update(std::span<const std::byte> data) noexcept;
	[[nodiscard]] std::array<uint8_t, 32> finish() noexcept;
	[[nodiscard]] static std::string hex(const std::array<uint8_t, 32>& digest);

private:
	void block(const uint8_t* chunk) noexcept;
	std::array<uint32_t, 8> state_;
	std::array<uint8_t, 64> buffer_{};
	uint64_t totalBytes_ = 0;
	size_t buffered_ = 0;
};

// Header facts read from a reference OTBM without loading tiles.
struct ReferenceHeader {
	uint32_t otbmVersion = 0;
	uint16_t width = 0;
	uint16_t height = 0;
	uint32_t itemsMajor = 0;
	uint32_t itemsMinor = 0;
	bool compressed = false;
	uint64_t fileBytes = 0;
};

// What the loaded client declares it can read.
struct ClientItemVersion {
	uint32_t itemsMajor = 0;
	uint32_t itemsMinor = 0;
	bool otbmSupported = false; // client lists this OTBM version as supported
};

enum class Compatibility : uint8_t {
	Compatible,
	NoClient,           // no client loaded; nothing can be classified
	OtbmUnsupported,    // client does not list the map's OTBM version
	ItemsMajorMismatch, // map was saved with another items.otb format
	ItemsMinorNewer,    // map expects a newer items.otb build than the client has
	EmptyHeader         // zero width/height or zero items version
};

// Pure decision. A newer minor is reported, never silently accepted; an older
// minor is compatible because unknown items are reported separately.
[[nodiscard]] Compatibility checkCompatibility(const ReferenceHeader& header,
	const ClientItemVersion* client) noexcept;

[[nodiscard]] const char* compatibilityName(Compatibility value) noexcept;

} // namespace rme::procedural
