// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/main.h"
#include "procedural/adapter/style_profile_store.h"

#include "app/client_version.h"
#include "app/managers/version_manager.h"
#include "procedural/core/reference_header.h"
#include "util/file_system.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <system_error>
#include <vector>

namespace rme::procedural {
namespace {

std::filesystem::path toPath(const wxString& text) { return std::filesystem::path(std::wstring(text.wc_str())); }

std::optional<std::string> sha256OfFile(const std::filesystem::path& file) {
	std::ifstream in(file, std::ios::binary);
	if (!in) return std::nullopt;
	Sha256 hash;
	std::vector<char> buffer(1u << 20);
	while (in) {
		in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
		const auto got = static_cast<size_t>(in.gcount());
		if (got) hash.update(std::as_bytes(std::span<const char>(buffer.data(), got)));
	}
	if (!in.eof()) return std::nullopt;
	return Sha256::hex(hash.finish());
}

} // namespace

std::optional<ClientAssetIds> loadedClientAssetIds() {
	if (!g_version.IsVersionLoaded()) return std::nullopt;
	const ClientVersion* version = g_version.getLoadedVersion();
	if (!version) return std::nullopt;
	const wxString modular = g_version.GetModularDataPath();
	if (modular.empty()) return std::nullopt;
	const auto otb = sha256OfFile(toPath(modular + "items.otb"));
	const auto dat = sha256OfFile(toPath(version->getMetadataPath().GetFullPath()));
	if (!otb || !dat) return std::nullopt;
	return ClientAssetIds { std::string(g_version.GetCurrentVersionIDView()), *otb, *dat };
}

std::filesystem::path styleProfileDirectory() {
	return toPath(FileSystem::GetLocalDataDirectory()) / "procedural" / "styles";
}

StyleProfileFolder scanStyleProfiles(const std::filesystem::path& folder, const ClientAssetIds& ids, size_t maxFiles, size_t maxFileBytes) {
	StyleProfileFolder result;
	result.path = folder;
	std::error_code ec;
	result.exists = std::filesystem::is_directory(folder, ec);
	if (!result.exists) return result;

	std::vector<std::filesystem::path> files;
	for (std::filesystem::directory_iterator it(folder, ec), end; !ec && it != end; it.increment(ec)) {
		std::error_code fileEc;
		if (it->is_regular_file(fileEc) && !fileEc && it->path().extension() == ".profile") files.push_back(it->path());
	}
	std::sort(files.begin(), files.end());
	if (files.size() > maxFiles) files.resize(maxFiles);

	std::vector<NamedProfileText> texts;
	for (const auto& file : files) {
		std::error_code sizeEc;
		const auto size = std::filesystem::file_size(file, sizeEc);
		std::ifstream in(file, std::ios::binary);
		if (sizeEc || size > maxFileBytes || !in) { ++result.unreadable; continue; }
		texts.push_back({file.stem().string(), std::string(std::istreambuf_iterator<char>(in), {})});
	}
	result.offer = offerProfiles(texts, ids.client, ids.otbSha256, ids.datSha256);
	return result;
}

} // namespace rme::procedural
