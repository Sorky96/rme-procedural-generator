// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/reference_header.h"

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

using namespace rme::procedural;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
	if (!ok) { ++failures; std::cerr << "FAIL: " << what << '\n'; }
}
std::string digestOf(std::string_view text, size_t chunk) {
	Sha256 hash;
	const auto* bytes = reinterpret_cast<const std::byte*>(text.data());
	for (size_t at = 0; at < text.size(); at += chunk) {
		hash.update({bytes + at, std::min(chunk, text.size() - at)});
	}
	return Sha256::hex(hash.finish());
}
}

int main() {
	// FIPS 180-4 / NIST vectors, split at several chunk sizes to cover buffering.
	for (const size_t chunk : {size_t {1}, size_t {7}, size_t {63}, size_t {64}, size_t {65}, size_t {4096}}) {
		check(digestOf("", chunk) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "empty");
		check(digestOf("abc", chunk) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "abc");
		check(digestOf("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", chunk)
			== "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1", "56-byte NIST");
	}
	check(digestOf(std::string(1'000'000, 'a'), 977) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0", "million a");

	const ClientItemVersion client {3, 62, true};
	ReferenceHeader header {2, 1024, 1024, 3, 62, false, 100};
	check(checkCompatibility(header, &client) == Compatibility::Compatible, "same version");
	header.itemsMinor = 40;
	check(checkCompatibility(header, &client) == Compatibility::Compatible, "older minor is compatible");
	header.itemsMinor = 63;
	check(checkCompatibility(header, &client) == Compatibility::ItemsMinorNewer, "newer minor");
	header.itemsMinor = 62; header.itemsMajor = 1;
	check(checkCompatibility(header, &client) == Compatibility::ItemsMajorMismatch, "major mismatch");
	header.itemsMajor = 3;
	check(checkCompatibility(header, nullptr) == Compatibility::NoClient, "no client");
	const ClientItemVersion noOtbm {3, 62, false};
	check(checkCompatibility(header, &noOtbm) == Compatibility::OtbmUnsupported, "otbm unsupported");
	header.width = 0;
	check(checkCompatibility(header, &client) == Compatibility::EmptyHeader, "empty header");

	if (failures == 0) std::cout << "REFERENCE_HEADER_PASS\n";
	return failures == 0 ? 0 : 1;
}
