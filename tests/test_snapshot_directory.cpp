#include "editor/persistence/snapshot_directory.h"

#include <array>
#include <fstream>
#include <iostream>
#include <set>
#include <thread>

int main() {
	const auto parent = std::filesystem::temp_directory_path();
	std::error_code ec;
	const auto root = reserveSnapshotDirectory(parent, L"rme_snapshot_test", ec);
	if (root.empty()) return 1;
	auto fail = [&](const char* message) {
		std::cerr << message << '\n';
		return 1;
	};

	const std::wstring base = L"map_żółć_snapshot";
	const auto existing = root / base;
	std::filesystem::create_directory(existing, ec);
	if (ec) return fail("Failed to prepare collision directory");
	const auto sentinel = existing / "existing.txt";
	{ std::ofstream output(sentinel); output << "preserved"; }
	{ std::ofstream output(root / (base + L"_1")); output << "file collision"; }

	std::array<std::filesystem::path, 8> reservations;
	std::array<std::error_code, 8> errors;
	std::array<std::thread, 8> workers;
	for (size_t i = 0; i < workers.size(); ++i) {
		workers[i] = std::thread([&, i] { reservations[i] = reserveSnapshotDirectory(root, base, errors[i]); });
	}
	for (auto& worker : workers) worker.join();
	std::set<std::filesystem::path> unique(reservations.begin(), reservations.end());
	if (unique.size() != workers.size() || unique.contains({})) return fail("Reservations were not unique");
	for (size_t i = 0; i < reservations.size(); ++i) {
		if (errors[i] || !std::filesystem::is_directory(reservations[i])) return fail("Reservation failed");
	}
	std::string contents;
	{ std::ifstream input(sentinel); input >> contents; }
	if (contents != "preserved") return fail("Existing snapshot was modified");
	{ std::ifstream input(root / (base + L"_1")); std::getline(input, contents); }
	if (contents != "file collision") return fail("Existing file was modified");

	const auto missing = reserveSnapshotDirectory(root / "missing-parent", base, ec);
	if (!missing.empty() || !ec) return fail("Missing parent was not reported");
	std::filesystem::remove_all(root, ec);
	if (ec) return fail("Failed to clean test directory");
	std::cout << "snapshot directory reservation PASS\n";
	return 0;
}
